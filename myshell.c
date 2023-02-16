#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "parser.h"
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <libgen.h>
//Código realizado por: David López Pereira y Antón Shvets

void mandatoCd(tline *line);
void masDeUno(tline *line);
void redirEntrada(tline *line);
void redirSalida(tline *line);
void redirError(tline *line);
void jobs();//No implementado
void fg();//No implementado

int main(void) {
	tline * line;
	char buf[1024];
	pid_t pid;
	int status;
	char *cd = "cd";
	//Duplicamos la entrada, salida y error estandar	
	int entrada= dup(0);
	int salida= dup(1);
	int error= dup(2);
	//Inhabilitamos la funcionalidad de Control C y Control \ para evitar que la minishell muera
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	printf("msh> ");	
	while (fgets(buf, 1024, stdin)) {	
		line = tokenize(buf);
		if (line==NULL) { //Si no añadimos nada continuara el bucle 
			continue;
		}
		if (line->redirect_input != NULL) {
			//Al introducir el símbolo < entrara en la funcion para redireccionar entrada
			redirEntrada(line);
		}
		if (line->redirect_output != NULL) {
			//Si introducimos > entrara en la funcion para redireccionar salida 			
			redirSalida(line);
		}
		if (line->redirect_error != NULL) {
			//Si introducimos 2> entrara en la funcion para redireccionar error
			redirError(line);
		}
		if(line-> ncommands == 1) {	
			if (strcmp(line->commands[0].argv[0], cd) == 0) {// Comprobamos cd
				//Habilitamos las señales que habíamos deshabilitado para que reaccionen con el proceso en foreground				
				signal(SIGINT, SIG_DFL);
				signal(SIGQUIT, SIG_DFL);		
				mandatoCd(line);
			}
			else {
				pid=fork();
				if(pid < 0) {
					fprintf(stderr,"Falló el fork(): %s\n", strerror(errno));
					exit(1);				
				}
				//Entramos en el proceso hijo
				else if(pid == 0) {
					//Habilitamos las señales para que reaccionen con el proceso hijo
					signal(SIGINT, SIG_DFL);
					signal(SIGQUIT, SIG_DFL);
					//Ejecutamos el comando introducido en la terminal 
					execvp(line->commands[0].argv[0],line->commands[0].argv);
					//Si la ejecución falla imprimira que se ha producido un error y saldra
					fprintf(stderr, "%s: No se encuentra el mandato\n",line->commands[0].argv[0]);
					exit(1);
				}
				else {
					//El proceso padre espera a que termine el hijo
					wait(&status);
				}
			}
		}
		else if (line -> ncommands > 1) {
		//Antes de ejecutar los comandos activaremos las señales para que reaccionen con los procesos en foreground
			masDeUno(line);

		}
		//Duplicar la anterior entrada en el descriptores estandar de entrada 0
		if (line->redirect_input != NULL) {
			dup2(entrada, 0);
		}
		//Duplicamos la anterior salida en el descriptor estandar de salida 1
		if (line->redirect_output != NULL) {
			dup2(salida, 1);
		}
		//Duplicamos el anterior error en el descriptor estandar de error 2
		if (line->redirect_error != NULL) {
			dup2(error, 2);
		}
		//Volvemos a inhabilitar Control C (SIGINT) y Control \ (SIGQUIT)
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		printf("msh> ");	
	}
	return 0;
}
//Cabecera de subprograma para ejecutar el mandato cd
void mandatoCd (tline *line) {
	char *dir; // necesario para poder guardar el directorio que nos pasan como argumento
	char buf[256];
	//No nos pueden pasar más de un argumento en el cd
	if (line -> commands[0].argc > 2) {
		fprintf (stderr, "Uso %s directorio\nError numero de argumentos ", line-> commands[0].argv[0]);
	}
	else if (line -> commands[0].argc == 1) {
		//Guardamos la variable $HOME en dir, si solo le pasamos cd 
		dir = getenv("HOME");
		if (dir == NULL) {
			fprintf (stderr,"No existe la variable $HOME\n");
		}
	}	
	else {
		dir = line-> commands[0].argv[1];
	}
	if (chdir(dir)!=0) {
		fprintf(stderr,"Error al cambiar de directorio. %s\n", strerror(errno));
	}
		
	printf ("El directorio actual es: %s\n", getcwd(buf, 256));			
}
//Cabecera de subprograma para ejecutar mas de un mandato
void masDeUno(tline *line) {
	int pid;
	int status;
	int nMandatos = line->ncommands;
	//Creamos una matriz de dos elementos, con los hijos como primer parametro y la tuberia como segundo
	int a[nMandatos][2];
	//Creamos todas las tuberias, tendremos que crear nMandatos - 1
	for(int i = 0; i<nMandatos-1; i++) {
		pipe(a[i]);
	}
	//Creamos todos los hijos necesarios
	for(int i = 0; i<nMandatos; i++) {
		pid = fork();
		if (pid < 0) {
			fprintf(stderr,"Error\n");
			exit(1);
		}	
		if (pid == 0) {
			//Habilitamos las señales para los procesos hijos 
			signal(SIGINT, SIG_DFL);
			signal(SIGQUIT, SIG_DFL);
			if (i == 0) {
			// En close 1 entrada de la tuberia , 0 salida de la tuberia	
			// En dup2 1 salida del proceso, 0 entrada del proceso
			close(a[i][0]);
			dup2(a[i][1], 1);
			}
			else if ((i > 0) && (i < nMandatos-1)) {				
				close(a[i-1][1]);
				close(a[i][0]);				
				dup2(a[i-1][0], 0);
				dup2(a[i][1], 1);
			}
			else {
				close(a[i-1][1]);
				dup2(a[i-1][0], 0);
			}
			
			for(int j = 0; j<nMandatos-1; j++) {
				close(a[j][0]);
				close(a[j][1]);
			}
			execvp(line->commands[i].filename, line->commands[i].argv);
			}
	}
		
	for(int i = 0; i<nMandatos-1; i++) {
	//Cerramos las tuberias abiertas del padre
		close(a[i][0]);
		close(a[i][1]);
	}
	//Esperamos a que terminen los dos procesos 
	for(int i = 0; i<nMandatos; i++) {
		wait(&status);
	}
}
//Los tres subprogramas lo que hacen es abrir el fichero que se ha dado, si no existe lo crea y obtiene su descriptor
void redirEntrada(tline *line) {
	int fde;
	//Abriremos en modo solo lectura y si no esta creado lo crearemos con O_CREAT, el modo lo hemois puesto en 0666 para que los ficheros tengan permiso de lectura y escritura por usuario, grupo y otros
	fde = open(line->redirect_input, O_CREAT | O_RDONLY, 0666);
	dup2(fde,0);
	close(fde);
}

void redirSalida(tline *line) {
	int fds;
	fds = open(line->redirect_output,  O_CREAT | O_RDWR | O_TRUNC, 0666);
	//En caso de error enviar un mensaje por pantalla y salir de la ejecución	
	if (fds == -1) {
		fprintf(stderr,"%s: Error. %s\n",line->redirect_output, strerror(errno));
	}
	else {
		dup2(fds, 1);
		close(fds);
		//Una vez duplicado el fds se puede cerrar 
	}	
}

void redirError(tline *line) {
	int fderr;
	fderr = open(line->redirect_error, O_CREAT | O_RDWR, 0666);
	//Al igual que en redirSalida si ocurre algún error enviar un mensaje por pantalla 	
	if (fderr == -1) {
		fprintf(stderr, "%s: Error. %s\n",line->redirect_error, strerror(errno));
	}
	else {
		dup2(fderr, 2);
		close(fderr);
	}
}
//Comandos no implementados 
void jobs(){}
void fg(){}

