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
#include <stdlib.h>
#include <time.h>
#include <unistd.h>


int total_nodes, mpi_rank;
Block *last_block_in_chain; //  Block *last_block_in_chain to const Block *last_block_in_chain;;
map<string,Block> node_blocks;
mutex primer;
mutex second;
bool listo = false;
bool bandera = false;
//mutex tercer[8];

bool verificar_y_migrar_cadena(const Block *rBlock, const MPI_Status *status){

	//TODO: Enviar mensaje TAG_CHAIN_HASH
	MPI_Request request;
	
	MPI_Isend((rBlock->block_hash),HASH_SIZE, MPI_CHAR, (status->MPI_SOURCE),TAG_CHAIN_HASH, MPI_COMM_WORLD, &request);
	Block *blockchain = new Block[VALIDATION_BLOCKS];  
	MPI_Status status2;

	status2.MPI_SOURCE = -1;
	while((status2.MPI_SOURCE != status->MPI_SOURCE) or (status2.MPI_TAG != TAG_CHAIN_RESPONSE)){
		MPI_Recv(blockchain, VALIDATION_BLOCKS, *MPI_BLOCK, MPI_ANY_SOURCE,  MPI_ANY_TAG, MPI_COMM_WORLD, &status2);
		if(status2.MPI_TAG == TAG_FIN){
			bandera = true;
			delete blockchain;	
			return 0;
		}
	}
	
	if((hashIguales(blockchain[0].block_hash, rBlock->block_hash) == false) or (blockchain[0].index != rBlock->index)){
		cout << "Cadena invalida por hash o index del primero" << endl;    
		delete []blockchain;
		return false;
	}  

	
	string hashr;
	block_to_hash(rBlock, hashr);
	string aux(rBlock->block_hash);
	if(hashr.compare(aux) != 0){
		cout << "Cadena invalida por funcion block to hash" << endl;
		delete [] blockchain;
		return false;
	}
	

	const Block* actual = rBlock;// indice de la blockchain 0
	int i = 1;
	while((i < VALIDATION_BLOCKS) and (actual->index > 1)){
		if((hashIguales(actual->previous_block_hash, blockchain[i].block_hash) == false) or ((actual->index-1) != blockchain[i].index )){
			cout << "Cadena invalida por enlaces o indices invalidos " << endl;
			delete[] blockchain;
			return false;
		}
		actual = &blockchain[i];
		i++;    
	}
		//Si llegamos acá la blockchain esta bien definida y es válida. 

		// Por ultimo, Hay que ver si termina en el indice 1 o alguno ya estaba en mi mapa, sino descarto. 
	
	int j = 0;
	bool res = false;
	std::map<string,Block>::iterator it;
	while((j < VALIDATION_BLOCKS) and (blockchain[j].index > 0) and !res){
		it = node_blocks.find(string(blockchain[j].block_hash));
 		if (it != node_blocks.end()){
 			res = res || true;//esto pasa siempre porque el receive tiene cola y el validate new block siempre agrega nuevos bloques
 		}
		j++;
	}

	if (blockchain[j-1].index == 1 || res != false){
		//printf("[%d] indice es %d y res es %d \n",mpi_rank,blockchain[j].index, res);
		int m = 0;
		while(blockchain[m].index > 0){
			node_blocks[string(blockchain[m].block_hash)] = blockchain[m];
			m++;
		}
		*last_block_in_chain = blockchain[0];
		cout <<"[" << mpi_rank <<"]"<<" termine de migrar la cadena" << endl;
		delete[] blockchain;
		return true;
	}
	
	delete []blockchain;
	return false;

}

bool validate_block_for_chain(const Block *rBlock, const MPI_Status *status){
	if(valid_new_block(rBlock)){

		node_blocks[string(rBlock->block_hash)]= *rBlock;

		//if (mpi_rank == 1 and ((rBlock-> index) > (last_block_in_chain->index)+2)){


			if( ((rBlock->index) == 1) and (last_block_in_chain->index == 0)){
				printf("[%d] Agregado a la lista bloque con index %d enviado por %d \n", mpi_rank, rBlock->index,status->MPI_SOURCE);
				*last_block_in_chain = *rBlock;  
				return true;
			}
			if(((rBlock-> index) == (last_block_in_chain->index)+1) and (hashIguales(rBlock->previous_block_hash, last_block_in_chain->block_hash)==true)){
				printf("[%d] Agregado a la lista bloque con index %d enviado por %d \n", mpi_rank, rBlock->index,status->MPI_SOURCE);
				*last_block_in_chain = *rBlock;
				return true;
			}   

			if(((rBlock-> index) == (last_block_in_chain->index)+1) and (hashIguales(rBlock->previous_block_hash,  last_block_in_chain->block_hash)== false)){
					printf("[%d] Perdí la carrera por uno (%d) contra %d \n", mpi_rank, rBlock->index, status->MPI_SOURCE);
					//tercer[mpi_rank].lock();
					bool res = verificar_y_migrar_cadena(rBlock,status);
					//tercer[mpi_rank].unlock();
					return res;
			}

			if((rBlock-> index) == (last_block_in_chain->index)){
				printf("[%d] Conflicto suave: Conflicto de branch (%d) contra %d \n",mpi_rank,rBlock->index,status->MPI_SOURCE);
				return false;
			}

			if((rBlock-> index) < (last_block_in_chain->index)){
				printf("[%d] Conflicto suave: Descarto el bloque (%d vs %d) contra %d \n",mpi_rank,rBlock->index,last_block_in_chain->index, status->MPI_SOURCE);
				return false;
			}

			if((rBlock-> index) > (last_block_in_chain->index) + 1){
				printf("[%d] Perdí la carrera por varios contra %d \n", mpi_rank, status->MPI_SOURCE);
				//tercer[mpi_rank].lock();
				bool res = verificar_y_migrar_cadena(rBlock,status);
				//tercer[mpi_rank].unlock();
				return res;
			} 
		return true;
		//}
	}
	printf("[%d] Error duro: Descarto el bloque recibido de %d porque no es válido \n",mpi_rank,status->MPI_SOURCE);
	return false;
}

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
}

//Proof of work
//TODO: Advertencia: puede tener condiciones de carrera
void* proof_of_work(void *ptr){

	//srand(time(NULL));
	//int num =1+rand()%(101-1);
	//if (mpi_rank == 1) sleep(100);

	string hash_hex_str;
	Block block;
	unsigned int mined_blocks = 0;
	while(true){

		if(listo){
			return NULL;
		}

		block = *last_block_in_chain;

		//Preparar nuevo bloque
		block.index += 1;
		block.node_owner_number = mpi_rank;
		block.difficulty = DEFAULT_DIFFICULTY;
		block.created_at = static_cast<unsigned long int> (time(NULL));
		memcpy(block.previous_block_hash,block.block_hash,HASH_SIZE);
		
		if(block.index > MAX_BLOCKS){
			int number=0;
			for(int i = 0; i < total_nodes; i++){
				if (i != mpi_rank){
					MPI_Send(&number, 1, MPI_INT, i, TAG_FIN, MPI_COMM_WORLD);
				}else{
					bandera = true;
				}
			}
			return NULL;
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
				printf("[%d] Miné un bloque con index %d \n",mpi_rank,last_block_in_chain->index);
				broadcast_block(last_block_in_chain);
						
			}
			primer.unlock();
		}

	}
	return NULL;
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
	pthread_t thread[1];
	pthread_create(&thread[0], NULL, proof_of_work, NULL );
	
	Block* blockr = new Block;
	char hash_hex_str[HASH_SIZE];
	MPI_Status status;
	MPI_Status status2;
	MPI_Request request;
	int flag;


	while(true){
		//if (mpi_rank == 1) sleep(5);
		if(bandera ==true){
			listo = true;
			pthread_join(thread[0], NULL);
			printf("Listo: %d\n", mpi_rank);
			delete last_block_in_chain;
			delete blockr;
			return 0;
		}

		MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);
		if(flag == true){
			if(status.MPI_TAG == TAG_NEW_BLOCK){
				MPI_Recv(blockr, 1, *MPI_BLOCK, MPI_ANY_SOURCE, TAG_NEW_BLOCK, MPI_COMM_WORLD, &status);
				primer.lock();
				validate_block_for_chain(blockr, &status);
				primer.unlock();
			}
			if(status.MPI_TAG==TAG_FIN){
				listo = true;
				pthread_join(thread[0], NULL);
				printf("Listo: %d\n", mpi_rank);
				delete last_block_in_chain;
				delete blockr;
				return 0;
			}
			if(status.MPI_TAG == TAG_CHAIN_HASH){
	  			MPI_Recv(&hash_hex_str, HASH_SIZE, MPI_CHAR, MPI_ANY_SOURCE, TAG_CHAIN_HASH, MPI_COMM_WORLD, &status2);
				Block actual = node_blocks[string(hash_hex_str)];
				Block *lista = new Block[VALIDATION_BLOCKS];      
				lista[0] = actual;       
				int i = 1;
				while((i  < VALIDATION_BLOCKS) and (actual.index > 0)){
					lista[i] = (node_blocks[string(actual.previous_block_hash)]);
					actual = node_blocks[string(actual.previous_block_hash)];
					i++;
				}
				MPI_Isend(lista, VALIDATION_BLOCKS, *MPI_BLOCK ,(status2.MPI_SOURCE), TAG_CHAIN_RESPONSE, MPI_COMM_WORLD, &request);       
				delete [] lista;
			}
		}
	}
}
	
