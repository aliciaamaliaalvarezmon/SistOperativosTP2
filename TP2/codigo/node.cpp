#include "node.h"
#include "picosha2.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <cstdlib>
#include <queue>
#include <atomic>
#include <mpi.h>
#include <map>
#include <mutex>
#include <iostream>
//esto es el la informacion del nodo(cantidad fija) (creo, agregado por Alicia)
int total_nodes, mpi_rank;
Block *last_block_in_chain; //  Block *last_block_in_chain to const Block *last_block_in_chain;;
map<string,Block> node_blocks;
mutex primer;
mutex segundo;
//mutex tercer[8];

//Cuando me llega una cadena adelantada, y tengo que pedir los nodos que me faltan
//Si nos separan más de VALIDATION_BLOCKS bloques de distancia entre las cadenas, se descarta por seguridad

//int MPI_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm)
bool verificar_y_migrar_cadena(const Block *rBlock, const MPI_Status *status){

	//TODO: Enviar mensaje TAG_CHAIN_HASH

 MPI_Send((rBlock->block_hash),HASH_SIZE, MPI_CHAR, (status->MPI_SOURCE),TAG_CHAIN_HASH, MPI_COMM_WORLD);

	Block *blockchain = new Block[VALIDATION_BLOCKS];  
	//TODO: Recibir mensaje TAG_CHAIN_RESPONSE
	//MPI_Status status2; 

	MPI_Recv(blockchain, VALIDATION_BLOCKS, *MPI_BLOCK, (status->MPI_SOURCE), TAG_CHAIN_RESPONSE,  MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	//TODO: Verificar que los bloques recibidos
	//sean válidos y se puedan acoplar a la cadena
		//delete []blockchain;
		//return true;

	if((!hashIguales(blockchain[0].block_hash, rBlock->block_hash)) or  (blockchain[0].index != rBlock->index) ){
		cout << "Cadena invalida por hash o index del primero" << endl;    
		delete []blockchain;
		return false;
	}  
	
	
	string hashr;
	block_to_hash(rBlock, hashr);

	if(hashr!= rBlock->block_hash){
		cout << "Cadena invalida por funcion block to hash" << endl;
		 delete [] blockchain;
		 return false;
	}

	const Block * actual = rBlock;
	int i = 1;
	while((i < VALIDATION_BLOCKS) and (actual->index >= 1)){
		if(valid_new_block(&blockchain[i])){
			if((!hashIguales(blockchain[i].block_hash, actual->previous_block_hash)) or (blockchain[i].index != (actual->index-1))){
			//	cout << "cadena invalida " << endl;
				delete[] blockchain;
				return false;
			}
		}else{
			//	cout << "cadena invalida por bloque invalido" << endl; 
			 delete[] blockchain;
			 return false;
		}
		actual = &blockchain[i]; //que ya vimos esta bien, creo, sino como obtenemos ?
		i++;    
	}
	//Si llegamos acá la blockchain esta bien definida y es válida

	//rBlock que es igual a blockchain[0]
	if(blockchain[0].index <= VALIDATION_BLOCKS){
		//cout <<"[" << mpi_rank <<"]"<<"cadena valida" << endl;
		int m = 0;
		while(blockchain[m].index >= 1){
				node_blocks[string(blockchain[m].block_hash)] = blockchain[m];
				m++;
		}
		*last_block_in_chain = blockchain[0];
		//cout <<"[" << mpi_rank <<"]"<<"termine de pasar cadena corta valida" << endl;
		delete[] blockchain;
		return true;
	}
	//Si llego aca la cadena era mayor a VALIDATION_BLOCKS, me tengo que fijar si encaja con la cadena de alice. 
	i = 0;
	
	Block* alice_actual = last_block_in_chain;
	
	int l = 0;
	//cadena_alice tiene la cadena del nodo actual
	while((l < VALIDATION_BLOCKS) and alice_actual->index > 0){		
		//cout <<"[" << mpi_rank <<"]"<<"cadena valida larga" << endl;
		for(int j = 0; j < VALIDATION_BLOCKS; j++ ){         
			if(hashIguales(alice_actual->block_hash, blockchain[j].block_hash)){//vemos por hash porque block no tiene ==
				for(int m = 0; m <= j; m++){
					node_blocks[string(blockchain[m].block_hash)] = blockchain[m];      
				}
			*last_block_in_chain = blockchain[0];
		//	cout <<"[" << mpi_rank <<"]"<<"termine de pasar cadena larga valida" << endl;     
			delete[] blockchain;      
		//	cout << "borro bien" << endl;
			return true;      
			}
		}
		alice_actual = &node_blocks[string(alice_actual->previous_block_hash)];
		l++;
	}


	delete []blockchain;
	return false;
}

//Verifica que el bloque tenga que ser incluido en la cadena, y lo agrega si corresponde
bool validate_block_for_chain(const Block *rBlock, const MPI_Status *status){
	if(valid_new_block(rBlock)){

		//Agrego el bloque al diccionario, aunque no
		//necesariamente eso lo agrega a la cadena
		node_blocks[string(rBlock->block_hash)]= *rBlock;

		//cout <<  "hashp que asocio al bloque" <<node_blocks[string(rBlock->block_hash)].previous_block_hash << endl;
		//cout << "hashp de bloque me llego"<< rBlock->previous_block_hash << endl;

		//TODO: Si el índice del bloque recibido es 1
		//y mí último bloque actual tiene índice 0,
		//entonces lo agrego como nuevo último.
			//printf("[%d] Agregado a la lista bloque con index %d enviado por %d \n", mpi_rank, rBlock->index,status->MPI_SOURCE);
			//return true;
		//MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank); Me parece que no debo usarlo aqui porque node lo llamo antes.......
		if( ((rBlock->index) == 1) and (last_block_in_chain->index == 0)){
			printf("[%d] Agregado a la lista bloque con index %d enviado por %d \n", mpi_rank, rBlock->index,status->MPI_SOURCE);
			*last_block_in_chain = *rBlock;  
			return true;
		}
		//TODO: Si el índice del bloque recibido es
		//el siguiente a mí último bloque actual,
		//y el bloque anterior apuntado por el recibido es mí último actual,
		//entonces lo agrego como nuevo último.
			//printf("[%d] Agregado a la lista bloque con index %d enviado por %d \n", mpi_rank, rBlock->index,status->MPI_SOURCE);
			//return true;
		if(((rBlock-> index) == (last_block_in_chain->index)+1) and hashIguales(rBlock->previous_block_hash, last_block_in_chain->block_hash)){
			printf("[%d] Agregado a la lista bloque con index %d enviado por %d \n", mpi_rank, rBlock->index,status->MPI_SOURCE);
			*last_block_in_chain = *rBlock;
			return true;
		}   

		//TODO: Si el índice del bloque recibido es
		//el siguiente a mí último bloque actual,
		//pero el bloque anterior apuntado por el recibido no es mí último actual,
		//entonces hay una blockchain más larga que la mía.
			//printf("[%d] Perdí la carrera por uno (%d) contra %d \n", mpi_rank, rBlock->index, status->MPI_SOURCE);
			//bool res = verificar_y_migrar_cadena(rBlock,status);
			//return res;
		if(((rBlock-> index) == (last_block_in_chain->index)+1) and !hashIguales(rBlock->previous_block_hash, last_block_in_chain->block_hash)){
				printf("[%d] Perdí la carrera por uno (%d) contra %d \n", mpi_rank, rBlock->index, status->MPI_SOURCE);
				//tercer[mpi_rank].lock();
				bool res = verificar_y_migrar_cadena(rBlock,status);
				//tercer[mpi_rank].unlock();
				return res;
		}


		//TODO: Si el índice del bloque recibido es igua al índice de mi último bloque actual,
		//entonces hay dos posibles forks de la blockchain pero mantengo la mía
			//printf("[%d] Conflicto suave: Conflicto de branch (%d) contra %d \n",mpi_rank,rBlock->index,status->MPI_SOURCE);
			//return false;
		if((rBlock-> index) == (last_block_in_chain->index)){
			printf("[%d] Conflicto suave: Conflicto de branch (%d) contra %d \n",mpi_rank,rBlock->index,status->MPI_SOURCE);
			return false;
		}

		//TODO: Si el índice del bloque recibido es anterior al índice de mi último bloque actual,
		//entonces lo descarto porque asumo que mi cadena es la que está quedando preservada.
			//printf("[%d] Conflicto suave: Descarto el bloque (%d vs %d) contra %d \n",mpi_rank,rBlock->index,last_block_in_chain->index, status->MPI_SOURCE);
			//return false;
		if((rBlock-> index) < (last_block_in_chain->index)){
			printf("[%d] Conflicto suave: Descarto el bloque (%d vs %d) contra %d \n",mpi_rank,rBlock->index,last_block_in_chain->index, status->MPI_SOURCE);
			return false;
		}

		//TODO: Si el índice del bloque recibido está más de una posición adelantada a mi último bloque actual,
		//entonces me conviene abandonar mi blockchain actual
			//printf("[%d] Perdí la carrera por varios contra %d \n", mpi_rank, status->MPI_SOURCE);
			//bool res = verificar_y_migrar_cadena(rBlock,status);
			//return res;
			if((rBlock-> index) > (last_block_in_chain->index) + 1){
			printf("[%d] Perdí la carrera por varios contra %d \n", mpi_rank, status->MPI_SOURCE);
			//tercer[mpi_rank].lock();
			bool res = verificar_y_migrar_cadena(rBlock,status);
			//tercer[mpi_rank].unlock();
			return res;
		} 
	}

	printf("[%d] Error duro: Descarto el bloque recibido de %d porque no es válido \n",mpi_rank,status->MPI_SOURCE);
	return false;
}

//int MPI_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm)
//Envia el bloque minado a todos los nodos
void broadcast_block(const Block *block){
	MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
	unsigned int cantidad_de_nodos_a_los_que_mensajee = 1; 
	unsigned int cant_de_nodos_yo_exclusive = total_nodes ;
	while (cantidad_de_nodos_a_los_que_mensajee < cant_de_nodos_yo_exclusive){
		unsigned int rank_a_mensajear = (mpi_rank + cantidad_de_nodos_a_los_que_mensajee) % total_nodes;
		MPI_Request request;
		MPI_Isend(block, 1, *MPI_BLOCK,rank_a_mensajear,TAG_NEW_BLOCK,MPI_COMM_WORLD, &request);//creo que en el taller hablamos de usar este no Mpi_Isend?   
		cantidad_de_nodos_a_los_que_mensajee++;  
	}
	//no a mi mismo
	//idea: va a enviar en circulo a todos los nodos que le siguen. (Mesa redonda)
}

//Proof of work
//TODO: Advertencia: puede tener condiciones de carrera
void* proof_of_work(void *ptr){
		string hash_hex_str;
		Block block;
		unsigned int mined_blocks = 0;
		while(true){
				
			block = *last_block_in_chain;

			//Preparar nuevo bloque
			block.index += 1;
			block.node_owner_number = mpi_rank;
			block.difficulty = DEFAULT_DIFFICULTY;
			block.created_at = static_cast<unsigned long int> (time(NULL));
			memcpy(block.previous_block_hash,block.block_hash,HASH_SIZE);

			if(block.index > MAX_BLOCKS){				
				Block* block = new Block;
				//MPI_Request request;
				for(int i = 0; i < total_nodes; i++){
					MPI_Send(block, 1, *MPI_BLOCK, i, TAG_FIN, MPI_COMM_WORLD);
				}
				delete block;
				break;
			}

			//Agregar un nonce al azar al bloque para intentar resolver el problema
			gen_random_nonce(block.nonce);

			//Hashear el contenido (con el nuevo nonce)
			block_to_hash(&block,hash_hex_str);

			//Contar la cantidad de ceros iniciales (con el nuevo nonce)
			if(solves_problem(hash_hex_str)){

					//Verifico que no haya cambiado mientras calculaba
					primer.lock();
					if(last_block_in_chain->index < block.index){
						mined_blocks += 1;
						*last_block_in_chain = block;
						strcpy(last_block_in_chain->block_hash, hash_hex_str.c_str());
						last_block_in_chain->created_at = static_cast<unsigned long int> (time(NULL));
						node_blocks[hash_hex_str] = *last_block_in_chain;          
						//TODO: Mientras comunico, no responder mensajes de nuevos nodos
						printf("[%d] Agregué un producido con index %d \n",mpi_rank,last_block_in_chain->index);						
						broadcast_block(last_block_in_chain);
					}
					primer.unlock();

			}
			
		}
		pthread_exit(NULL);
		//return NULL;
}

int node(){

	//Tomar valor de mpi_rank y de nodos totales
	MPI_Comm_size(MPI_COMM_WORLD, &total_nodes);
	MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);

	//La semilla de las funciones aleatorias depende del mpi_ranking
	srand(time(NULL) + mpi_rank);
	printf("[MPI] Lanzando proceso %u\n", mpi_rank);

	last_block_in_chain = new Block;

	//Inicializo el primer bloque
	last_block_in_chain->index = 0;
	last_block_in_chain->node_owner_number = mpi_rank;
	last_block_in_chain->difficulty = DEFAULT_DIFFICULTY;
	memset(last_block_in_chain->previous_block_hash,0,HASH_SIZE);

	//TODO: Crear thread para minar //Punto 2

	pthread_t thread[1]; //se supone que la thread mina mientras esto escucha?y la otra escucha  
 	pthread_create(&thread[0], NULL, proof_of_work, NULL );//me parece que el parametro que se le pasa a prood_of_work no importa;
	 
	Block* blockr = new Block;
	char hash_hex_str[HASH_SIZE];
	MPI_Status status;
	MPI_Request request;
	//MPI_Status status2;
	MPI_Status status3;
	int flag;

	//Aca ya hay 2 threads una va a prrof_of_work y otra sigue el codigo
	//Thread que escucha;
	while(true){
		//TODO: Recibir mensajes de otros nodos   
		//TODO: Si es un mensaje de nuevo bloque, llamar a la función
		// validate_block_for_chain con el bloque recibido y el estado de MPI
		MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);
		if(flag == true){
			if(status.MPI_TAG == TAG_NEW_BLOCK){
				MPI_Recv(blockr, 1, *MPI_BLOCK, MPI_ANY_SOURCE, TAG_NEW_BLOCK, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				primer.lock();
				validate_block_for_chain(blockr, &status);
				primer.unlock();
			}
			if(status.MPI_TAG==TAG_FIN){

				//segundo.lock();
				//MPI_Recv(blockr, 1, *MPI_BLOCK, MPI_ANY_SOURCE, TAG_FIN, MPI_COMM_WORLD, MPI_STATUS_IGNORE);				
+				pthread_join(thread[0], NULL);					
				printf("Listo: %d\n", mpi_rank);
				delete last_block_in_chain;		
				delete blockr;			
				//segundo.unlock();	
				return 0;
			}

			if(status.MPI_TAG == TAG_CHAIN_HASH){
				// cout << mpi_rank<<"llega" << endl;
				//TODO: Si es un mensaje de pedido de cadena,
				//responderlo enviando los bloques correspondientes          
				MPI_Recv(&hash_hex_str, sizeof(hash_hex_str), MPI_CHAR, MPI_ANY_SOURCE, TAG_CHAIN_HASH, MPI_COMM_WORLD, &status3);
				//primer.unlock();     
			 	Block actual = node_blocks[string(hash_hex_str)];       
				Block *lista = new Block[VALIDATION_BLOCKS];      
				lista[0] = actual;       
			 	int i = 1;      
			 	while((i  < VALIDATION_BLOCKS) and (actual.index > 0)){
					lista[i] = (node_blocks[string(actual.previous_block_hash)]);
				 	actual = node_blocks[string(actual.previous_block_hash)];
					i++;
			 	}
				//armo lista de validation_block blockes y la mando
			 
				MPI_Isend(lista, VALIDATION_BLOCKS, *MPI_BLOCK ,status3.MPI_SOURCE, TAG_CHAIN_RESPONSE, MPI_COMM_WORLD, &request);       
				delete [] lista;
			}
		}
	}
	 //MPI_ANI_SOURCE recibe mensajes desde cualquier emisor
}
	

	/*
		MPI_Iprobe( MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);
	 	if((flag == true) and primer.try_lock()){          
			if(status.MPI_TAG == TAG_NEW_BLOCK and tercer[mpi_rank].try_lock()){           
			MPI_Recv(&blockr, sizeof(blockr), *MPI_BLOCK, MPI_ANY_SOURCE, TAG_NEW_BLOCK, MPI_COMM_WORLD, &status2);
			tercer[mpi_rank].unlock();     
			primer.unlock();  
			 validate_block_for_chain(&blockr, &status);
			}else if(status.MPI_TAG == TAG_CHAIN_HASH){
			 	// cout << mpi_rank<<"llega" << endl;
				//TODO: Si es un mensaje de pedido de cadena,
				//responderlo enviando los bloques correspondientes          
				MPI_Recv(&hash_hex_str, sizeof(hash_hex_str), MPI_CHAR, MPI_ANY_SOURCE, TAG_CHAIN_HASH, MPI_COMM_WORLD, &status3);
				primer.unlock();     
			 	Block actual = node_blocks[string(hash_hex_str)];       
				Block *lista = new Block[VALIDATION_BLOCKS];      
				lista[0] = actual;       
			 	int i = 1;      
			 	while((i  < VALIDATION_BLOCKS) and (actual.index > 0)){
					lista[i] = (node_blocks[string(actual.previous_block_hash)]);
				 	actual = node_blocks[string(actual.previous_block_hash)];
					i++;
			 	}
				//armo lista de validation_block blockes y la mando
			 
				MPI_Send(lista, VALIDATION_BLOCKS, *MPI_BLOCK ,status3.MPI_SOURCE, TAG_CHAIN_RESPONSE, MPI_COMM_WORLD);       
				delete [] lista;
		 }


	*/