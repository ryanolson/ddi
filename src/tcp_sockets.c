/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Wrapper subroutines for common TCP/IP socket calls.
 *
 * Author: Ryan M. Olson
 * CVS $Id: tcp_sockets.c,v 1.2 2006/06/07 16:22:42 ryan Exp $
\* -------------------------------------------------------------------- */
 # include "ddi_base.h"
 # if defined DDI_SOC


/* -------------------------------------------------------------------- *\
   TCP/IP Wrapper function for connect
   ===================================
   Returns -1 on an error rather than calling Fatal_error.
\* -------------------------------------------------------------------- */
   int Connect(int s, const struct sockaddr *name, socklen_t namelen) {
      int value;
      if((value = connect(s,name,namelen)) == -1) {
         switch(errno) {
           case EINTR: return Connect(s,name,namelen);
           case EBADF: fprintf(stdout," TCP connect error: EBADF.\n"); break;
           case EFAULT: fprintf(stdout," TCP connect error: EFAULT.\n"); break;
           case ENOTSOCK: fprintf(stdout," TCP connect error: ENOTSOCK.\n"); break;
           case ETIMEDOUT: fprintf(stdout," TCP connect error: ETIMEDOUT.\n"); break;
           case ENETUNREACH: fprintf(stdout," TCP connect error: ENETUNREACH.\n"); break;
           case ECONNREFUSED: fprintf(stdout," TCP connect error: ECONNREFUSED.\n"); break;
           case EADDRNOTAVAIL: fprintf(stdout," TCP connect error: EADDRNOTAVAIL.\n"); break;
           default: fprintf(stdout," TCP connect error: Unknown error message.\n");
         }
      }
      fflush(stdout);
      return value;
   }


/* ------------------------------------------------------- *\
   TCP/IP Wrapper function for accept
   ==================================
\* ------------------------------------------------------- */
   int Accept(int socket,struct sockaddr *address,socklen_t *address_len) {
      int value;
      if((value = accept(socket,address,address_len)) == -1) {
         switch(errno) {
            case EINTR: return Accept(socket,address,address_len); break;
            case EBADF: fprintf(stdout," TCP accept error: EBADF.\n"); break;
            case EFAULT: fprintf(stdout," TCP accept error: EFAULT.\n"); break;
            case EMFILE: fprintf(stdout," TCP accept error: EMFILE.\n"); break;
            case ENFILE: fprintf(stdout," TCP accept error: ENFILE.\n"); break;
            case ENOMEM: fprintf(stdout," TCP accept error: ENOMEM.\n"); break;
            case EINVAL: fprintf(stdout," TCP accept error: EINVAL.\n"); break;
            case ENOBUFS: fprintf(stdout," TCP accept error: ENOBUFS.\n"); break;
            case ENOTSOCK: fprintf(stdout," TCP accept error: ENOTSOCK.\n"); break;
            case EOPNOTSUPP: fprintf(stdout," TCP accept error: EOPNOTSUPP.\n"); break;
            case ECONNABORTED: fprintf(stdout," TCP accept error: ECONNABORTED.\n"); break;
            default: fprintf(stdout," TCP accept error: Unknow error message.\n"); break;
         }
         Fatal_error(911);
      }
      return value;
   }


/* --------------------------------------------------------------------- *\
   TCP/IP Wrapper function for recv
   ================================
   This function is most similar to recv with the flag MSG_WAITALL which
   is not necessarily implemented on all platforms.  Recv return only 
   after the entire message of size len has been received or it returns
   a value of -1 if the socket receives an EOF.  Recv call Fatal_error 
   for all other types of errors.
\* --------------------------------------------------------------------- */ 
   ssize_t Recv(int s, void *buf, size_t len, int flags) {
      char *buff = (char *) buf;
      ssize_t nbytes = len;   
      ssize_t nrecvd;
   
      while(nbytes > 0) {
         nrecvd = recv(s,buff,nbytes,flags);
         if(nrecvd <= 0) {
            if(errno == EINTR) continue;
          # if !defined DDI_MPI
            if(nrecvd == 0) return -1;
          # endif
            fprintf(stdout,"%s: TCP Error in Recv.\n",DDI_Id());
            switch(errno) {
               case EBADF:    fprintf(stdout," TCP recv error: EBADF.\n");    break;
               case ENOTCONN: fprintf(stdout," TCP recv error: ENOTCONN.\n"); break;
               case ENOTSOCK: fprintf(stdout," TCP recv error: ENOTSOCK.\n"); break;
               case EAGAIN:   fprintf(stdout," TCP recv error: EAGAIN.\n");   break;
               case EFAULT:   fprintf(stdout," TCP recv error: EFAULT.\n");   break; 
               default:
                  fprintf(stdout," TCP recv error: Unknown.\n");
                  fprintf(stdout," TCP recv error: return value errno=%d\n"); 
                  break;
            }
            Fatal_error(911);
         }
         
         buff   += nrecvd;
         nbytes -= nrecvd;
                  
         DEBUG_OUT(LVL9,(stdout,"%s: %lu bytes received; %lu remaining.\n",DDI_Id(),nrecvd,nbytes))

       }

       return (ssize_t) len;
   }


/* --------------------------------------------------- *\
   TCP/IP Wrapper function for send
   ================================
\* --------------------------------------------------- */
   ssize_t Send(int s,const void *msg,size_t len,int flags) {
      ssize_t nbytes = send(s,msg,len,flags);
      if(nbytes == -1) {
         switch(errno) {
            case EBADF:        fprintf(stdout," TCP send error: EBADF.\n");        break;
            case EFAULT:       fprintf(stdout," TCP send error: EFAULT.\n");       break;
            case ENOTSOCK:     fprintf(stdout," TCP send error: ENOTSOCK.\n");     break;
            case EMSGSIZE:     fprintf(stdout," TCP send error: EMSGSIZE.\n");     break;
            case EHOSTUNREACH: fprintf(stdout," TCP send error: EHOSTUNREACH.\n"); break;
            default:
               fprintf(stdout," TCP send error: Unknown.\n");
               fprintf(stdout," TCP send error: return value errno=%d\n"); 
               break;
         }
         Fatal_error(911);
      }
      return nbytes;
   }

 # else

/* ------------------------------------------- *\
   Just so we don't have an empty object file.
\* ------------------------------------------- */
   static int dummy = 0;

 # endif
