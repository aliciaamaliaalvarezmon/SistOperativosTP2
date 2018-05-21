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
//esto es el la informacion del nodo(cantidad fija) (creo, agregado por Alicia)
int total_nodes, mpi_rank;
Block *last_block_in_chain;
map<string,Block> node_blocks;

//Cuando me llega una cadena adelantada, y tengo que pedir los nodos que me faltan
//Si nos separan más de VALIDATION_BLOCKS bloques de distancia entre las cadenas, se descarta por seguridad
bool verificar_y_migrar_cadena(const Block *rBlock, const MPI_Status *status){

  //TODO: Enviar mensaje TAG_CHAIN_HASH

  Block *blockchain = new Block[VALIDATION_BLOCKS];

  //TODO: Recibir mensaje TAG_CHAIN_RESPONSE

  //TODO: Verificar que los bloques recibidos
  //sean válidos y se puedan acoplar a la cadena
    //delete []blockchain;
    //return true;


  delete []blockchain;
  return false;
}

//Verifica que el bloque tenga que ser incluido en la cadena, y lo agrega si corresponde
bool validate_block_for_chain(const Block *rBlock, const MPI_Status *status){
  if(valid_new_block(rBlock)){

    //Agrego el bloque al diccionario, aunque no
    //necesariamente eso lo agrega a la cadena
    node_blocks[string(rBlock->block_hash)]=*rBlock;

    //TODO: Si el índice del bloque recibido es 1
    //y mí último bloque actual tiene índice 0,
    //entonces lo agrego como nuevo último.
      //printf("[%d] Agregado a la lista bloque con index %d enviado por %d \n", mpi_rank, rBlock->index,status->MPI_SOURCE);
      //return true;

    //TODO: Si el índice del bloque recibido es
    //el siguiente a mí último bloque actual,
    //y el bloque anterior apuntado por el recibido es mí último actual,
    //entonces lo agrego como nuevo último.
      //printf("[%d] Agregado a la lista bloque con index %d enviado por %d \n", mpi_rank, rBlock->index,status->MPI_SOURCE);
      //return true;

    //TODO: Si el índice del bloque recibido es
    //el siguiente a mí último bloque actual,
    //pero el bloque anterior apuntado por el recibido no es mí último actual,
    //entonces hay una blockchain más larga que la mía.
      //printf("[%d] Perdí la carrera por uno (%d) contra %d \n", mpi_rank, rBlock->index, status->MPI_SOURCE);
      //bool res = verificar_y_migrar_cadena(rBlock,status);
      //return res;


    //TODO: Si el índice del bloque recibido es igua al índice de mi último bloque actual,
    //entonces hay dos posibles forks de la blockchain pero mantengo la mía
      //printf("[%d] Conflicto suave: Conflicto de branch (%d) contra %d \n",mpi_rank,rBlock->index,status->MPI_SOURCE);
      //return false;

    //TODO: Si el índice del bloque recibido es anterior al índice de mi último bloque actual,
    //entonces lo descarto porque asumo que mi cadena es la que está quedando preservada.
      //printf("[%d] Conflicto suave: Descarto el bloque (%d vs %d) contra %d \n",mpi_rank,rBlock->index,last_block_in_chain->index, status->MPI_SOURCE);
      //return false;

    //TODO: Si el índice del bloque recibido está más de una posición adelantada a mi último bloque actual,
    //entonces me conviene abandonar mi blockchain actual
      //printf("[%d] Perdí la carrera por varios contra %d \n", mpi_rank, status->MPI_SOURCE);
      //bool res = verificar_y_migrar_cadena(rBlock,status);
      //return res;

  }

  printf("[%d] Error duro: Descarto el bloque recibido de %d porque no es válido \n",mpi_rank,status->MPI_SOURCE);
  return false;
}

//int MPI_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm)
//Envia el bloque minado a todos los nodos
void broadcast_block(const Block *block){
  //No enviar a mí mismo  
  //TODO: Completar
  unsigned int cantidad_de_nodos_a_los_que_mensajee = 1; 
  unsigned int cant_de_nodos_yo_exclusive = total_nodes -1;
  while (cantidad_de_nodos_a_los_que_mensajee < cant_de_nodos_yo_exclusive){
    unsigned int rank_a_mensajear = (mpi_rank + cantidad_de_nodos_a_los_que_mensajee) % total_nodes;
    MPI_Send(&block, sizeof(block), *MPI_BLOCK,rank_a_mensajear,TAG_NEW_BLOCK,MPI_COMM_WORLD);
    cantidad_de_nodos_a_los_que_mensajee++;
  }//no a mi mismo
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
      memcpy(block.previous_block_hash,block.block_hash,HASH_SIZE);

      //Agregar un nonce al azar al bloque para intentar resolver el problema
      gen_random_nonce(block.nonce);

      //Hashear el contenido (con el nuevo nonce)
      block_to_hash(&block,hash_hex_str);

      //Contar la cantidad de ceros iniciales (con el nuevo nonce)
      if(solves_problem(hash_hex_str)){

          //Verifico que no haya cambiado mientras calculaba
          if(last_block_in_chain->index < block.index){
            mined_blocks += 1;
            *last_block_in_chain = block;
            strcpy(last_block_in_chain->block_hash, hash_hex_str.c_str());
            last_block_in_chain->created_at = static_cast<unsigned long int> (time(NULL));
            node_blocks[hash_hex_str] = *last_block_in_chain;
            printf("[%d] Agregué un producido con index %d \n",mpi_rank,last_block_in_chain->index);

            //TODO: Mientras comunico, no responder mensajes de nuevos nodos
            broadcast_block(last_block_in_chain);
          }
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
  last_block_in_chain->created_at = static_cast<unsigned long int> (time(NULL));
  memset(last_block_in_chain->previous_block_hash,0,HASH_SIZE);

  //TODO: Crear thread para minar //Punto 2

  pthread_t thread[2]; //se supone que la thread mina mientras esto escucha?y la otra escucha  
 
    pthread_create(&thread[0], NULL, proof_of_work, NULL );//me parece que el parametro que se le pasa a prood_of_work no importa;
    pthread_join(thread[0], NULL);


  //  pthread_t thread[realnt];
  //int tid;  
  //for(tid = 0; tid <  realnt; tid++  ){
  //  pthread_create(&thread[tid], NULL, maxola, &aux );//le pasa a max el struct Hashcontador, con nuestro hash y la thread    
  //}
  //for (tid = 0; tid < realnt; ++tid){
  //      pthread_join(thread[tid], NULL);
  //  }Ejemplo sacado de tp1 




  while(true){



      //TODO: Recibir mensajes de otros nodos
    Block* blockr;
    char* hash_hex_str[32];
    MPI_Status status;
    //TODO: Si es un mensaje de nuevo bloque, llamar a la función
    // validate_block_for_chain con el bloque recibido y el estado de MPI
    if( MPI_Recv(blockr, sizeof(MPI_BLOCK), *MPI_BLOCK, MPI_ANY_SOURCE, TAG_NEW_BLOCK, MPI_COMM_WORLD, &status)){
      validate_block_for_chain(blockr, &status);
    }
    //TODO: Si es un mensaje de pedido de cadena,
    //responderlo enviando los bloques correspondientes
    if(MPI_Recv(hash_hex_str, sizeof(hash_hex_str), MPI_CHAR, MPI_ANY_SOURCE, TAG_CHAIN_HASH, MPI_COMM_WORLD, &status)){
      //armo lista de validation_block blockes y la mando
     // MPI_Send(lista, sizeof(lista), ,mpi_rank, TAG_CHAIN_RESPONSE, MPI_COMM_WORLD);
    }
   //MPI_ANI_SOURCE recibe mensajes desde cualquier emisor, no se si esta bien, pero bueno.
  }

  delete last_block_in_chain;
  return 0;
}