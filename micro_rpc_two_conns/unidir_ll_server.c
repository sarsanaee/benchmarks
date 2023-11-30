#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <tas_ll.h>


#include "unidir_ll_simple.h"

#include "../common/microbench.h"

// uint64_t send_tcp_message(struct flextcp_context *ctx, struct flextcp_connection *conn)
// {
//   void *buf;
//   uint64_t ret;
//   uint32_t sent;

//   ret = flextcp_connection_tx_alloc(conn, MSG_SIZE - allocated, &buf);
//   if (ret < 0)
//   {
//     fprintf(stderr, "flextcp_connection_tx_alloc failed\n");
//     exit(-1);
//   }

//   allocated += ret;

//   memset(buf, 1, ret);

//   if (allocated < MSG_SIZE)
//   {
//     // printf("allocated %lu bytes, %lu ret\n", allocated, ret);
//     return 0;
//   }

//   assert(allocated == MSG_SIZE);

//   // sending should be separated.
//   sent = flextcp_connection_tx_send(ctx, conn, allocated);
//   if (sent != 0)
//   {
//     fprintf(stderr, "flextcp_connection_tx_send failed\n");
//     exit(-1);
//   }
//   else
//   {
//     ret = allocated;
//     allocated = 0;
//     return ret;
//   }
// }

struct params params;

int main(int argc, char *argv[])
{
  // create two pthread objects
  pthread_t thread1, thread2;

  if (parse_params(argc, argv, &params) != 0)
  {
    return -1;
  }


  if (flextcp_init())
  {
    fprintf(stderr, "flextcp_init failed\n");
    return -1;
  }


  if (params.client)
  {
    struct context client_ctx = {
      .server_ip = "10.0.255.100",
      .server_port = 1234,
      .client_port = 1235,
      .params = &params,
    };

    printf("client\n");

    pthread_create(&thread1, NULL, (void *)client, &client_ctx);
    // printf("Press any key to continue\n");
    // getchar();
    sleep(2);
    pthread_create(&thread2, NULL, (void *)server, &client_ctx);
  }

  if (params.server)
  {
    printf("server\n");

    struct context server_ctx = {
      .server_ip = "10.0.255.102",
      .server_port = 1235,
      .client_port = 1234,
      .params = &params,
    };


    pthread_create(&thread1, NULL, (void *)server, &server_ctx);
    // printf("Press any key to continue\n");
    getchar();
    pthread_create(&thread2, NULL, (void *)client, &server_ctx);
  }

  pthread_join(thread1, NULL);
  pthread_join(thread2, NULL);

  return 0;
}

