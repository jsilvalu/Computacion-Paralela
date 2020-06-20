#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <CL/cl.h>
#include <mpi.h>
#include "FuncionesOCL.h"


typedef struct {
	double  x,  // Coordenada x del punto
		    y;  // Coordenada y del punto
} punto_t;

void initialize(double *m,int N, punto_t *mesh, int P, double lr, double ur, double lm, double um){
	int i,j;
	double space;

	//Los valores del polinomio se general aleatoriamente
	for(i=0;i<N*N;i++)  
		m[i] =((1.*rand())/RAND_MAX)*(ur-lr)+lr;
	// Los puntos de la malla se generan equidistantes
	space=(um-lm)/(P-1);
	for(i=0;i<P;i++)
		for(j=0;j<P;j++) {
			mesh[i*P+j].x=lm+i*space;
			mesh[i*P+j].y=lm+j*space;
		}
}

void escribir(double *m, int t){
	int i, j;

	for (i = 0; i < t; i++) {
		for (j = 0; j < t; j++)
			printf("%.2lf ",m[i*t+j]);
		printf("\n");
	}
	printf("\n");
}

void escribirm(punto_t *m, int t, int exp){
	int i, j;
	printf("Coordenadas x de la malla del experimento %d:\n", exp);
	for (i = 0; i < t; i++) {
		for (j = 0; j < t; j++)
			printf("%.2f ",m[i*t+j].x);
		printf("\n");
	}
	printf("\nCoordenadas y de la malla del experimento %d:\n", exp);
	for (i = 0; i < t; i++) {
		for (j = 0; j < t; j++)
			printf("%.2f ",m[i*t+j].y);
		printf("\n");
	}
	printf("\n");
}

/*
c
c     mseconds - returns elapsed milliseconds since Jan 1st, 1970.
c
*/
long long mseconds(){
	struct timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec*1000 + t.tv_usec/1000;
}

int ObtenerParametros(int argc, char *argv[], int *debug, int *num_workitems, int *workitems_por_workgroups) {
	int i;
	*debug=0;
	*num_workitems=0;
	*workitems_por_workgroups=0;
	if (argc<2)
		return 0;
	for (i=2; i<argc;) {
		if (strcmp(argv[i], "-d")==0) {
			*debug=1;
			i++;
		}
		else if (strcmp(argv[i], "-wi")==0) {
			i++;
			if (i==argc)
				return 0;
			*num_workitems=atoi(argv[i]);
			i++;
			if (*num_workitems<=0)
				return 0;
		}
		else if (strcmp(argv[i], "-wi_wg")==0) {
			i++;
			if (i==argc)
				return 0;
			*workitems_por_workgroups=atoi(argv[i]);
			i++;
			if (*workitems_por_workgroups<=0)
				return 0;
		}
		else
			return 0;
	}
	return 1;
}



typedef struct {
	cl_platform_id *plataformas;
	cl_device_id *dispositivos;
	cl_context contexto;
	cl_command_queue cola;
	cl_program programa;
	cl_kernel kernel_potencia, kernel_funcion;
} EntornoOCL_t;



// **************************************************************************
// ***************************** IMPLEMENTACIóN *****************************
// **************************************************************************
cl_int InicializarEntornoOCL(EntornoOCL_t *entorno) {

	cl_uint num_platformas=1, num_dispositivos=1;

	//Obtención de las plataformas
	ObtenerPlataformas(entorno->plataformas, num_platformas);
	//Obtención de los dispositivos
	ObtenerDispositivos(entorno->plataformas[0], CL_DEVICE_TYPE_ALL, entorno->dispositivos, num_dispositivos);
	//Creación del contexto, video tema 2 - contextos y colas
	CrearContexto(entorno->plataformas[0], entorno->dispositivos, num_dispositivos, entorno->contexto);
	//Creación de la cola,   video tema 2 - contextos y colas
	CrearCola(entorno->contexto, entorno->dispositivos[0], CL_QUEUE_PROFILING_ENABLE, entorno->cola);
	//Creación del progra,a, Video Tema 2 - Programas y Kernels
	CrearPrograma(entorno->programa, entorno->contexto, num_dispositivos, entorno->dispositivos, "", "programa.cl");
	CrearKernel(entorno->kernel_potencia, entorno->programa, "potencias");
	CrearKernel(entorno->kernel_funcion,  entorno->programa, "funcion");
}

cl_int LiberarEntornoOCL(EntornoOCL_t *entorno) {

	//Liberacion de todos los campos del registro EntornoOCL_t para OpenCL
	free(entorno->plataformas);					//Libero cl_platform_id *plataformas;
	clReleaseDevice(entorno->dispositivos[0]);  //Libero cl_device_id *dispositivos;
	clReleaseContext(entorno->contexto);        //Libero cl_context contexto;
	clReleaseCommandQueue(entorno->cola);       //Libero cl_command_queue cola;
	clReleaseProgram(entorno->programa);        //Libero cl_program programa;
	clReleaseKernel(entorno->kernel_potencia);  //Libero cl_kernel kernel para potencia;
	clReleaseKernel(entorno->kernel_funcion);   //Libero cl_kernel kernel para funcion;
}

void prepararAsignacionParametros_Potencia(EntornoOCL_t entorno, cl_mem buffer_malla, cl_mem buffer_MatrizXY, int N, cl_char modo){

	AsignarParametro(entorno.kernel_potencia, 0, sizeof(cl_mem), &buffer_malla);
	AsignarParametro(entorno.kernel_potencia, 1, sizeof(cl_mem), &buffer_MatrizXY);
	AsignarParametro(entorno.kernel_potencia, 2, sizeof(cl_int), &N);
	AsignarParametro(entorno.kernel_potencia, 3, sizeof(cl_char), &modo);
}

void prepararAsignacionParametros_Funcion(EntornoOCL_t entorno, int P, int N, cl_mem buffer_A_MatrizCoeficientes, cl_mem buffer_MatrizPotenciasX, cl_mem buffer_MatrizPotenciasY, cl_mem buffer_Vectorresultado) {

	AsignarParametro(entorno.kernel_funcion, 0, sizeof(cl_int), &P);
	AsignarParametro(entorno.kernel_funcion, 1, sizeof(cl_int), &N);
	AsignarParametro(entorno.kernel_funcion, 2, sizeof(cl_mem), &buffer_A_MatrizCoeficientes);
	AsignarParametro(entorno.kernel_funcion, 3, sizeof(cl_mem), &buffer_MatrizPotenciasX);
	AsignarParametro(entorno.kernel_funcion, 4, sizeof(cl_mem), &buffer_MatrizPotenciasY);
	AsignarParametro(entorno.kernel_funcion, 5, sizeof(cl_mem), &buffer_Vectorresultado);
}
/*
Implementaci�n secuencial
N -> Tama�o de la matriz de coeficientes (NxN)
A -> Matriz de coeficientes
P -> Tama�o de la malla de puntos
malla -> Malla de coordenahnvcdas (x,y) de tama�o P
r -> Vector resultado de tama�o P
*/

void FUNCION_OCL(int N, double *A, int P,  EntornoOCL_t entorno, cl_double *Mx, cl_double *My, bool ejecucion, double *r, punto_t *malla) {

	cl_mem buffer_MatrizPotenciasX, buffer_MatrizPotenciasY, buffer_Vectorresultado, buffer_A_MatrizCoeficientes;
		
	//Campo de evento de ejecución del kernel que se pasa por parámetros en cadda invocación de ejecución del Kernel
	cl_event evento_ejecKernel;

	//Hay que cambiar el tipo del tamaño de malla de puntos porque no se puede enviar por parámtros.
	//Además, hay que declararlo como constante porque sino podría cambiar su valor.
	const size_t tamMallaPts = P;

	//Creo los buffers para la matriz de potencias X y la matriz de potencias Y
	CrearBuffer(entorno.contexto, CL_MEM_USE_HOST_PTR, P*N*sizeof(double), Mx, buffer_MatrizPotenciasX);
	CrearBuffer(entorno.contexto, CL_MEM_USE_HOST_PTR, P*N*sizeof(double), My, buffer_MatrizPotenciasY);
	
	if(ejecucion){
	
		cl_mem buffer_malla;
		CrearBuffer(entorno.contexto, CL_MEM_USE_HOST_PTR, P*sizeof(punto_t), malla, buffer_malla);

		//Asigno los parámetros para poder invocar a la función del kernel "potencia" en el mismo orden
		prepararAsignacionParametros_Potencia(entorno, buffer_malla, buffer_MatrizPotenciasX, N, 1);
		EjecutarKernel(entorno.cola, entorno.kernel_potencia, 1, NULL, &tamMallaPts, NULL, 0, NULL, evento_ejecKernel);

		//Asigno los parámetros para poder invocar a la función del kernel "potencia" en el mismo orden
		prepararAsignacionParametros_Potencia(entorno, buffer_malla, buffer_MatrizPotenciasY, N, 0);
		EjecutarKernel(entorno.cola, entorno.kernel_potencia, 1, NULL, &tamMallaPts, NULL, 0, NULL, evento_ejecKernel);

	}else{
		//Creo los buffers para el vector resultado y la matriz de coeficientes
		CrearBuffer(entorno.contexto, CL_MEM_USE_HOST_PTR, P*sizeof(double), r, buffer_Vectorresultado);
		CrearBuffer(entorno.contexto, CL_MEM_USE_HOST_PTR, N*N*sizeof(double), A, buffer_A_MatrizCoeficientes);

		//Calculo mediante la función "funcion" del kernel del vector resultado
		prepararAsignacionParametros_Funcion(entorno, P, N, buffer_A_MatrizCoeficientes, buffer_MatrizPotenciasX, buffer_MatrizPotenciasY, buffer_Vectorresultado); 
		EjecutarKernel(entorno.cola, entorno.kernel_funcion, 1, NULL, &tamMallaPts, NULL, 0, NULL, evento_ejecKernel);
	}

	clFinish(entorno.cola);

}


// **************************************************************************
// *************************** FIN IMPLEMENTACIóN ***************************
// **************************************************************************

/*
Recibir� los siguientes par�metros (los par�metros entre corchetes son opcionales): fichEntrada [-d] [-wi work_items] [-wi_wg workitems_por_workgroup]
fichEntrada -> Obligatorio. Fichero de entrada con los par�metros de lanzamiento de los experimentos
-d -> Opcional. Si se indica, se mostrar�n por pantalla los valores iniciales, finales y tiempo de cada experimento
-wi work_items -> Opcional. Si se indica, se lanzar�n tantos work items como se indique en work_items (para OpenCL)
-wi_wg workitems_por_workgroup -> Opcional. Si se indica, se lanzar�n tantos work items en cada work group como se indique en WorkItems_por_WorkGroup (para OpenCL)
*/
int main(int argc,char *argv[]) {
	int i,
		debug=0,				     // Indica si se desean mostrar los tiempos y resultados parciales de los experimentos
		num_workitems=0, 		     // N�mero de work items que se utilizar�n
		workitems_por_workgroups=0,  // N�mero de work items por cada work group que se utilizar�n
		N,					         // Tama�o de la matriz de coeficientes A (NxN)
		P,					         // N�mero de puntos de la malla (PxP)
		cuantos,				     // N�mero de experimentos a realizar
		semilla,				     // Semilla para la generaci�n de n�meros aleatorios
		myrank,				         // Identificador del proceso
		size;				   		 // N�mero de procesos lanzados
	double *A, 				   		 // Matriz de coeficientes. Se representa en forma de vector. Para acceder a la fila f y la columna c: A[f*N+c]
		  *r,     			   		 // Resultado (PxP)
		  lr,				   		 // Valor m�nimo de los coeficientes
		  ur,				   		 // Valor m�ximo de los coeficientes
		  lm,				   		 // Valor m�nimo de la malla
		  um;				   		 // Valor m�ximo de la malla
	punto_t *malla;			   		 // Malla de puntos (PxP)
	long long ti,				     // Tiempo inicial
			tf,				   		 // Tiempo final
			tt=0;			   		 // Tiempo acumulado de los tiempos parciales de todos los experimentos realizados
	
	FILE *f;					   	 // Fichero con los datos de entrada

	
	if (!ObtenerParametros(argc, argv, &debug, &num_workitems, &workitems_por_workgroups)) {
		printf(ANSI_COLOR_RED "[ERROR!] Ejecucion incorrecta\nEl formato correcto es %s fichEntrada [-d] [-wi work_items] [-wi_wg workitems_por_workgroup]" ANSI_COLOR_RESET "\n", argv[0]);
		return 0;
	}
	
	//Inicialización del enterno MPI, obtención del RANK y TAMAÑO
	MPI_Init(&argc,&argv);
  	MPI_Comm_rank(MPI_COMM_WORLD,&myrank);
  	MPI_Comm_size(MPI_COMM_WORLD,&size);
	
	//Inicializamos el entorno (Apartado de implementacion)
	EntornoOCL_t entorno;
	InicializarEntornoOCL(&entorno);


	// Se leen el n�mero de experimentos a realizar
	if(myrank==0) { // S�lo el proceso 0 tiene acceso al fichero y, por tanto, a los datos
		f=fopen(argv[1],"r");
		fscanf(f, "%d",&cuantos);
	}
	ti=mseconds(); 

	// ****************************************************************************
	// ***************************** IMPLEMENTACI�N *******************************
	// ****************************************************************************

	MPI_Bcast(&cuantos, 1, MPI_INT, 0, MPI_COMM_WORLD); //Envio numero experimentos

	// ****************************************************************************
	// *************************** FIN IMPLEMENTACI�N *****************************
	// ****************************************************************************
	tf=mseconds(); 
	tt+=tf-ti;
	
	for(i=0;i<cuantos;i++)
	{
		if(myrank==0) { // S�lo el proceso 0 tiene acceso al fichero y, por tanto, a los datos
			fscanf(f, "%d",&N);       // Tama�o de la matriz de coeficientes A (NxN)
			fscanf(f, "%d",&P);       // N�mero de puntos de la malla (PxP)
			fscanf(f, "%d",&semilla); // Semilla para la generacion aleatoria de los datos
			fscanf(f, "%lf",&lr);	 // Valor m�nimo de los coeficientes
			fscanf(f, "%lf",&ur);	 // Valor m�ximo de los coeficientes
			fscanf(f, "%lf",&lm);	 // Valor m�nimo de la malla
			fscanf(f, "%lf",&um);	 // Valor m�ximo de la malla

			// Reserva de memoria para los coeficientes, los puntos en la malla y el resultado
			A = (double *) calloc(sizeof(double),N*N);
			malla = (punto_t *) malloc(sizeof(punto_t)*P*P);
			r=(double *) malloc(sizeof(double)*P*P);


			srand(semilla);
			initialize(A,N,malla,P,lr,ur,lm,um);

			if (debug) {
				printf("Matriz de coeficientes del experimento %d:\n", i); escribir(A,N);
				escribirm(malla, P, i);
			}
		}
		
		ti=mseconds(); 
// **************************************************************************
// ***************************** IMPLEMENTACI�N *****************************
// **************************************************************************

cl_double *MatrizPotenciasX, *MatrizPotenciasY;

//Creación de estructuras para almacenar 'trozos' de información de cada proceso
double *Trozo_MatrizPotenciasX = (double *) malloc(sizeof(double)*(P*P/size)*N);
double *Trozo_MatrizPotenciasY = (double *) malloc(sizeof(double)*(P*P/size)*N);

//Reservado dinámico de vector de reales para almacenar el RESULTADO
double *vectorResultado =( double *) malloc(sizeof(double)*P*P/size);


		// El proceso 0 debe repartir la informaci�n a procesar entre todos los procesos (incluido �l mismo)
		
		if(myrank==0){ 

			MatrizPotenciasX = (cl_double *) malloc(sizeof(cl_double)*N*P*P);
		    MatrizPotenciasY = (cl_double *) malloc(sizeof(cl_double)*N*P*P);

			// calculo en paralelo las potencias
			FUNCION_OCL(N, A, P*P, entorno, MatrizPotenciasX, MatrizPotenciasY, true, NULL, malla);

			//Multidifusión de datos almacenados en el buffer (1º parámetro) a los demás procesos
			MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
			MPI_Bcast(A, N*N, MPI_DOUBLE, 0, MPI_COMM_WORLD);
			MPI_Bcast(&P, 1, MPI_INT, 0, MPI_COMM_WORLD);

			//Reparto automáticamente los datos entre los procesos de un comunicador por orden
			MPI_Scatter(MatrizPotenciasX, (P*P/size)*N, MPI_DOUBLE, Trozo_MatrizPotenciasX, (P*P/size)*N, MPI_DOUBLE, 0, MPI_COMM_WORLD);
			MPI_Scatter(MatrizPotenciasY, (P*P/size)*N, MPI_DOUBLE, Trozo_MatrizPotenciasY, (P*P/size)*N, MPI_DOUBLE, 0, MPI_COMM_WORLD);

			// calculo final de resultados 
			FUNCION_OCL(N, A, P*P/size, entorno, Trozo_MatrizPotenciasX, Trozo_MatrizPotenciasY, false, vectorResultado, NULL);
			
			int i, j;

			for(i=0; i<P*P/size;i++) // Copiado de datos en el vector
			 	r[i] = vectorResultado[i];

			//Recibo de los datos de los trozos
			for(i=1; i < size ; i++){
				MPI_Bcast(vectorResultado, P*P/size, MPI_DOUBLE, 0, MPI_COMM_WORLD);
				for(j=0; j < (P*P/size) ; j++)  //Almacenamiento de los resultados
					r[j] = vectorResultado[j];
			}

		}else{
		
			A = (double *) malloc(sizeof(double)*N*N);

			//Recepción de los datos para el cálculo final
			MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);    
			MPI_Bcast(A, N*N, MPI_DOUBLE, 0, MPI_COMM_WORLD);
			MPI_Bcast(&P, 1, MPI_INT, 0, MPI_COMM_WORLD);

			//Reparto automáticamente los datos entre los procesos de un comunicador por orden
			MPI_Scatter(MatrizPotenciasX, (P*P/size)*N, MPI_DOUBLE, Trozo_MatrizPotenciasX, (P*P/size)*N, MPI_DOUBLE, 0, MPI_COMM_WORLD);
			MPI_Scatter(MatrizPotenciasY, (P*P/size)*N, MPI_DOUBLE, Trozo_MatrizPotenciasY, (P*P/size)*N, MPI_DOUBLE, 0, MPI_COMM_WORLD);

			FUNCION_OCL(N, A, P*P/size, entorno, Trozo_MatrizPotenciasX, Trozo_MatrizPotenciasY, false, vectorResultado, NULL);

			//envio del resultado parcial	
			MPI_Bcast(vectorResultado, P*P/size, MPI_DOUBLE, 0, MPI_COMM_WORLD);
		}

		//Liberación de todas las estructuras creadas incluyendo las usadas para almacenar 'trozos'	
		free(MatrizPotenciasX);
		free(MatrizPotenciasY);
		free(Trozo_MatrizPotenciasX);
		free(Trozo_MatrizPotenciasY);
		free(vectorResultado);
	
	
// *********************************************************************************************************************************************
// *************************** FIN IMPLEMENTACI�N **********************************************************************************************
// *********************************************************************************************************************************************
		tf=mseconds(); 
		tt+=tf-ti;
		
		if (myrank==0){
			if (debug) {
				printf("\n" ANSI_COLOR_CYAN);
				printf("======================================================================\n");
				printf("=========  Tiempo del experimento %d: %Ld ms\n", i, tf-ti);
				printf("======================================================================\n\n" ANSI_COLOR_RESET);
				printf(" · Resultado del experimento %d:\n\n", i); escribir(r, P);
			}
			free(A);
			free(malla);
			free(r);
		}
    }
    
    LiberarEntornoOCL(&entorno);
	MPI_Finalize();
	if (myrank==0){
		printf("\n" ANSI_COLOR_CYAN);
		printf("======================================================================\n");
		printf("=========  Tiempo total de %d experimentos: %Ld ms\n", cuantos, tt);
		printf("====================================================================== FIN \n\n" ANSI_COLOR_RESET);
		fclose(f);
	}
	return 0;
}
