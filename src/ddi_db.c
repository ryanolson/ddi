#include "ddi_base.h"

#ifndef MAX_DB_ENTRIES
#define MAX_DB_ENTRIES 1000
#endif

#ifndef MAX_DB_MSG_SIZE
#define MAX_DB_MSG_SIZE 2097152 
#endif

static FILE *db_file      = NULL;
static int db_entry_count = 0;
static long db_eof_offset = 0;
static DB_Entry *db_entry = NULL;

void DB_Init() {
   DebugOutput(10);
   MAX_DEBUG((stdout,"%s: openning database file.\n",DDI_Id()))
   DebugOutput(0);
   db_file = fopen("database.ddi","w+");
   if(db_file == NULL) {
      fprintf(stdout,"%s: Error in fopen.\n",DDI_Id());
      Fatal_error(911);
   }

   db_entry = Malloc(MAX_DB_ENTRIES*sizeof(int));
   MAX_DEBUG((stdout,"%s: DB_Init Finished.\n",DDI_Id()))
}

void DB_Close() {
   if(db_file)  fclose(db_file);
   if(db_entry) free(db_entry);
}

int DB_Create(size_t size) {
   char ack;
   const DDI_Comm *comm = Comm_find(DDI_COMM_WORLD);
   DDI_Patch req;

   req.oper = DB_CREATE_ENTRY;
   req.size = size;

   Send(gv(sockets)[comm->np],(void *) &req,sizeof(DDI_Patch),0);
   Recv(gv(sockets)[comm->np],(void *) &ack,1,0);

   DDI_Recv(&req,sizeof(DDI_Patch),comm->np);

   return req.handle;
}

void DB_Create_server(DDI_Patch *req,int from) {

   size_t i;
   char zero  = 0;
   int handle = db_entry_count++;
   
   req->handle = handle;
   db_entry[handle].offset = db_eof_offset;
   db_entry[handle].size   = req->size;

   MAX_DEBUG((stdout,"%s: record of length %li starting at %li.\n",DDI_Id(),req->size,db_eof_offset))
   fseek(db_file,db_eof_offset,0);
   for(i=0; i<req->size; i++) fwrite(&zero,1,1,db_file);

   db_eof_offset += req->size;
   if(db_eof_offset != ftell(db_file)) {
      fprintf(stdout,"error: db_eof_offset (%li) != ftell(db_file) (%li).\n",db_eof_offset,ftell(db_file));
      Fatal_error(911);
   }

   DDI_Send(req,sizeof(DDI_Patch),from); 
   MAX_DEBUG((stdout,"%s: DB_Create_server Finished.\n",DDI_Id()))
}


void DB_Read(int handle,size_t size,void *buffer) {

   char ack;
   char *pbuffer = NULL;
   size_t working_size,remaining;
   DDI_Patch req;
   DDI_Comm *comm = Comm_find(DDI_COMM_WORLD);

   req.handle = handle;
   req.size   = size;
   req.oper   = DB_READ_ENTRY;

   Send(gv(sockets)[comm->np],(void *) &req, sizeof(DDI_Patch), 0);
   Recv(gv(sockets)[comm->np],(void *) &ack, 1, 0);

   pbuffer      = (char *) buffer;
   remaining    = size;
   working_size = MAX_DB_MSG_SIZE;

   while(remaining) {
      if(remaining < working_size) working_size = remaining;
      DDI_Recv(pbuffer,working_size,comm->np);
      
      remaining -= working_size;
      pbuffer   += working_size;
   }

}

void DB_Read_server(DDI_Patch *req,int from) {

   int handle = req->handle;
   void *buffer = NULL;
   size_t remaining,working_size;

   if(handle < 0 || handle >= db_entry_count) {
      fprintf(stdout,"%s: process %i is requesting an invalid db entry (%i).\n",DDI_Id(),from,handle);
      Fatal_error(911);
   }

   if(req->size > db_entry[handle].size) {
      fprintf(stdout,"%s: process %i attempting to read %li bytes from record %i which is only %li bytes.\n",DDI_Id(),from,req->size,handle,db_entry[handle].size);
      Fatal_error(911);
   }

   fseek(db_file,db_entry[handle].offset,0);
   DDI_Memory_push(MAX_DB_MSG_SIZE,&buffer,NULL);

   remaining    = req->size;
   working_size = MAX_DB_MSG_SIZE;

   while(remaining) {
      if(remaining < working_size) working_size = remaining;
      fread((char *) buffer,1,working_size,db_file);
      DDI_Send(buffer,working_size,from);
      remaining -= working_size;
   }

   DDI_Memory_pop(MAX_DB_MSG_SIZE);

}
   
void DB_Write(int handle,size_t size,void *buffer) {

   char ack;
   DDI_Patch req;
   char *pbuffer = NULL;
   size_t remaining,working_size;
   DDI_Comm *comm = Comm_find(DDI_COMM_WORLD);

   req.handle = handle;
   req.size   = size;
   req.oper   = DB_WRITE_ENTRY;

   Send(gv(sockets)[comm->np],(void *) &req, sizeof(DDI_Patch), 0);
   Recv(gv(sockets)[comm->np],(void *) &ack, 1, 0);

   pbuffer      = (char *) buffer;
   remaining    = size;
   working_size = MAX_DB_MSG_SIZE;

   while(remaining) {
      if(remaining < working_size) working_size = remaining;
      DDI_Send(pbuffer,working_size,comm->np);
      remaining -= working_size;
      pbuffer   += working_size;
   }

   Recv(gv(sockets)[comm->np],(void *) &ack, 1, 0);

}

void DB_Write_server(DDI_Patch *req,int from) {

   char ack=37;
   int handle = req->handle;
   void *buffer = NULL;
   size_t remaining,working_size;

   if(handle < 0 || handle >= db_entry_count) {
      fprintf(stdout,"%s: process %i is requesting an invalid db entry (%i).\n",DDI_Id(),from,handle);
      Fatal_error(911);
   }

   if(req->size > db_entry[handle].size) {
      fprintf(stdout,"%s: process %i attempting to write %li bytes to record %i which is only %li bytes.\n",DDI_Id(),from,req->size,handle,db_entry[handle].size);
      Fatal_error(911);
   }

   fseek(db_file,db_entry[handle].offset,0);
   DDI_Memory_push(MAX_DB_MSG_SIZE,&buffer,NULL);

   remaining    = req->size;
   working_size = MAX_DB_MSG_SIZE;

   while(remaining) {
      if(remaining < working_size) working_size = remaining;
      DDI_Recv(buffer,working_size,from);
      fwrite((char *) buffer,1,working_size,db_file);
      remaining -= working_size;
   }

   DDI_Memory_pop(MAX_DB_MSG_SIZE);
   Send(gv(sockets)[from],(void *) &ack, 1, 0);

}


