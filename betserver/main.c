//
//  main.c
//  betserver
//
//  Created by Stepan Rakhimov on 30/09/2018.
//  Copyright © 2018 Stepan Rakhimov. All rights reserved.
//

#include <arpa/inet.h>
#include <errno.h>
//#include <resolv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <sys/fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "protocol.h"

#define BETSERVER_PORT 2222
#define BETSERVER_MESSAGE_SIZE_MAX 31
#define BETSERVER_MESSAGE_HEADER_SIZE sizeof(unsigned int)

#define BETSERVER_NUM_CLIENTS 40
#define BETSERVER_NUM_MIN 0xe0ffff00
#define BETSERVER_NUM_MAX 0xe0ffffaa
#define BETSERVER_CLIENT_ID_MAX 0xffff //65535
#define BETSERVER_BET_PERIOD 15
#define BETSERVER_TICK_MS 100

struct client {
  int unique_id;
  int socket_fd;
  unsigned int bet;
  unsigned int bet_done;
};

struct client clients[BETSERVER_NUM_CLIENTS];

int generate_unique_id()
{
  return rand() % BETSERVER_CLIENT_ID_MAX;
}

/* zero clients array */
void init_clients()
{
  bzero(clients, sizeof(clients));
}

/* returns first unused index or -1 if not found */
int get_free_client_index()
{
  for(int i=0; i<BETSERVER_NUM_CLIENTS; ++i)
  {
    if (0 == clients[i].socket_fd) return i;
  }
  /* no free slots */
  return -1;
}

/* returns client index for unique id, -1 if not found */
int get_client_index_by_id(int unique_id)
{
  for (int i=0; i<BETSERVER_NUM_CLIENTS; ++i)
  {
    if(clients[i].unique_id == unique_id) return i;
  }
  return -1;
}

/* returns client index for unique id, -1 if not found */
int get_client_index_by_socketfd(int socket)
{
  for (int i=0; i<BETSERVER_NUM_CLIENTS; ++i)
  {
    if(clients[i].socket_fd == socket) return i;
  }
  return -1;
}

/* returns client unique id for socket fd, -1 if not found */
int get_client_uniqueid_by_socketfd(int socket)
{
  for (int i=0; i<BETSERVER_NUM_CLIENTS; ++i)
  {
    if(clients[i].socket_fd == socket) return clients[i].unique_id;
  }
  return -1;
}

int count_active_clients()
{
  int res = 0;
  for (int i=0; i< BETSERVER_NUM_CLIENTS; ++i)
  {
    if (0 != clients[i].unique_id) ++res;
  }
  return res;
}

int count_betting_clients()
{
  int res = 0;
  for (int i=0; i<BETSERVER_NUM_CLIENTS; ++i)
  {
    if (0 != clients[i].bet_done) ++ res;
  }
  return res;
}

/* returns client's unique id or -1 if failed */
int create_client(int socket_fd)
{
  int index = get_free_client_index();
  if (-1 == index) return -1;
  clients[index].socket_fd = socket_fd;
  int new_id = generate_unique_id();
  while(-1 != get_client_index_by_id(new_id))
  {
    new_id = generate_unique_id();
  }
  clients[index].unique_id = new_id;
  printf("new client %d created for socket %d at index %d\n", new_id, socket_fd, index);
  printf("active clients: %d\n", count_active_clients());
  return new_id;
}

/* returns client's index or -1 if not found */
int set_client_bet(int unique_id, unsigned int bet)
{
  int index = get_client_index_by_id(unique_id);
  if (-1 == index) return -1;
  clients[index].bet = bet;
  clients[index].bet_done = 1;
  printf("bet %u set for client %d\n", bet, unique_id);
  printf("betting clients: %d\n", count_betting_clients());
  return index;
}

int remove_client_by_index(int index)
{
  int unique_id = clients[index].unique_id;
  clients[index].socket_fd=0;
  clients[index].unique_id=0;
  clients[index].bet=0;
  clients[index].bet_done=0;
  printf("client %d removed at index %d\n", unique_id, index);
  printf("active clients: %d\n", count_active_clients());
  return index;
}

int remove_client_by_socketfd(int socket)
{
  int index = get_client_index_by_socketfd(socket);
  if (-1 == index) return -1;
  return remove_client_by_index(index);
}

int remove_client(int unique_id)
{
  int index = get_client_index_by_id(unique_id);
  if (-1 == index) return -1;
  return remove_client_by_index(index);
}

int handle_bet_message(int socket_fd, char* buffer, size_t size)
{
  int uid = get_client_uniqueid_by_socketfd(socket_fd);
  if (-1 == uid) return -1;
  if (size != sizeof(int)) return -1;
  unsigned int* b = (unsigned int*) buffer;
  unsigned int bet = ntohl(*b);
  printf("got %u\n", bet);
  if (bet < BETSERVER_NUM_MIN || bet > BETSERVER_NUM_MAX)
  {
    remove_client(uid);
    return -1;
  }
  set_client_bet(uid, bet);
  return uid;
}

void error(const char *msg)
{
  perror(msg);
  exit(errno);
}

volatile sig_atomic_t done = 0;
void term(int signum)
{
  done = 1;
}

//void set_socket_reuse(int socket)
//{
//  int yes = 1;
//  if(setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
//  {
//    error("setsockopt failed");
//  }
//}
//
//void set_socket_nonblock(int socket) {
//  int flags;
//  flags = fcntl(socket, F_GETFL, 0);
//  fcntl(socket, F_SETFL, flags | O_NONBLOCK);
//}

unsigned int roll_the_dice()
{
  return BETSERVER_NUM_MIN + rand() / (RAND_MAX / (BETSERVER_NUM_MAX - BETSERVER_NUM_MIN + 1) + 1);
}

void setup_sigterm_handling()
{
  struct sigaction action;
  bzero(&action, sizeof(struct sigaction));
  action.sa_handler = term;
  sigaction(SIGTERM, &action, NULL);
}

int init_server_socket(fd_set* sockets, int* sockets_size)
{
  int server_socket;

  /* Create streaming socket */
  if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    error("socket create failed");
  }

  /* Initialize address/port structure */
  struct sockaddr_in server;
  bzero(&server, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons(BETSERVER_PORT);
  server.sin_addr.s_addr = INADDR_ANY;

  /* Assign a port number to the socket */
  if (bind(server_socket, (struct sockaddr*)&server, sizeof(server)) != 0)
  {
    error("socket:bind failed");
  }

  /* Make it a "listening socket". Limit to BETSERVER_NUM_CLIENTS connections */
  if (listen(server_socket, BETSERVER_NUM_CLIENTS) != 0)
  {
    error("socket:listen failed");
  }

  /* add the listener to the master set */
  FD_SET(server_socket, sockets);
  /* keep track of the biggest file descriptor */
  *sockets_size = server_socket; /* so far, it's this one*/

  return server_socket;
}

void handle_winner(const unsigned int winner, const int listen_socket, fd_set* sockets, const int sockets_size) {
  for(int j = 0; j <= sockets_size; j++)
  {
    /* send to everyone! */
    if(FD_ISSET(j, sockets) && (j != listen_socket))
    {
      char win[50];
      sprintf(win, "winner is %u\n", winner);
      if(send(j, win, strlen(win), 0) == -1)
      {
        perror("send failed");
      }
      /* forget client */
      int index = get_client_index_by_socketfd(j);
      if (-1 == index)
      {
        close(j);
        FD_CLR(j, sockets);
      }
      else
      {
        if(clients[index].bet_done)
        {
          if(clients[index].bet == winner)
          {
            printf("client %d has won\n", clients[index].unique_id);
            char* msg = "You have won!\n";
            if(send(j, msg, strlen(msg), 0) == -1)
            {
              perror("send failed");
            }
          }
          remove_client_by_index(index);
          close(j);
          FD_CLR(j, sockets);
        }
      }
    }
  }
}

size_t recv_msg(int i, void* buffer, size_t size, fd_set* sockets)
{
  size_t nbytes;
  if((nbytes = recv(i, buffer, size, 0)) <= 0)
  {
    /* got error or connection closed by client */
    if(nbytes == 0)
    /* connection closed */
      printf("socket %d hung up\n", i);
    else
      perror("recv failed");

    /* close it... */
    close(i);
    /* remove from master set */
    FD_CLR(i, sockets);
    /* forget client */
    remove_client_by_socketfd(i);
  }
  return nbytes;
}

void handle_updated_sockets(fd_set* sockets, const fd_set* sockets_updated, int* sockets_size, const int listen_socket)
{
  /* message buffer */
  char buffer[BETSERVER_MESSAGE_SIZE_MAX];
  bzero(buffer, BETSERVER_MESSAGE_SIZE_MAX);
  printf("sockets size: %d\n", *sockets_size);

  for(int i = 0; i <= *sockets_size; i++)
  {
    if(FD_ISSET(i, sockets_updated))
    { /* we got one... */
      if(i == listen_socket)
      {
        int client_socket;
        struct sockaddr_in client_addr;
        bzero(&client_addr, sizeof(client_addr));
        socklen_t addrlen=sizeof(client_addr);

        /* accept an incomming connection  */
        client_socket = accept(listen_socket, (struct sockaddr*)&client_addr, &addrlen);
        if (client_socket == -1)
        {
          perror("client connect failed");
        }
        else
        {
          FD_SET(client_socket, sockets); /* add to master set */
          if(client_socket > *sockets_size)
          { /* keep track of the maximum */
            *sockets_size = client_socket;
          }
          printf("%s:%d connected\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
          if (-1 == create_client(client_socket))
          {
            close(client_socket);
            FD_CLR(client_socket, sockets);
            perror("client create failed, disconnect");
          }
        }
      }
      else
      {
        /* handle data from a client */
        unsigned int header = 0;
        size_t nbytes = recv_msg(i, &header, BETSERVER_MESSAGE_HEADER_SIZE, sockets);
        if(nbytes > 0)
        {
          struct msg_header message_header = parse_message_header(ntohl(header));
          unsigned int extra_message_size = message_header.size - BETSERVER_MESSAGE_HEADER_SIZE;
          if (message_header.type == BETSERVER_OPEN && 0 == extra_message_size) continue;
          if (message_header.type == BETSERVER_BET && extra_message_size > 0)
          {
            nbytes = recv_msg(i, &buffer, extra_message_size, sockets);
            if(nbytes > 0)
            {
              if (-1 == handle_bet_message(i, buffer, nbytes))
              {
                {
                  close(i);
                  FD_CLR(i, sockets);
                  remove_client_by_socketfd(i);
                }
              }
            }
          }
          else
          {
            close(i);
            FD_CLR(i, sockets);
            remove_client_by_socketfd(i);
          }
        }
      }
    }
  }
}

static int check_updated_sockets(fd_set *sockets, fd_set *sockets_updated, int sockets_size) {
  /* set timeout for select() */
  struct timeval select_timeout;
  bzero(&select_timeout, sizeof(select_timeout));
  select_timeout.tv_usec = BETSERVER_TICK_MS * 1000;

  /* copy open sockets */
#ifndef FD_COPY
  *sockets_updated = *sockets;
#else
  FD_COPY(sockets, sockets_updated);
#endif
  /* get updated sockets */
  int selected = select(sockets_size+1, sockets_updated, NULL, NULL, &select_timeout);
  if(selected == -1)
  {
    error("select failed");
  }
  return selected;
}

int main(int argc, const char * argv[]) {
  puts("Betserver started.");
  fflush(stdout);

  /* handle SIGTERM */
  setup_sigterm_handling();

  /* initialize clients storage */
  init_clients();

  /* sockets descriptor set */
  fd_set sockets;
  /* helper sockets descriptor set */
  fd_set sockets_updated;
  /* current sockets set size */
  int sockets_size;
  /* clear the master and temp sets */
  FD_ZERO(&sockets);
  FD_ZERO(&sockets_updated);

  /* server socket */
  int listen_socket = init_server_socket(&sockets, &sockets_size);
//  set_socket_reuse(listen_socket);
//  set_socket_nonblock(listen_socket);

  /* save current time for later use */
  time_t previous_time;
  time(&previous_time);
  
  /* Server runs till SIGTERM */
  while (!done)
  {
    fflush(stdout);

    time_t current_time;
    double seconds = difftime(time(&current_time), previous_time);
    if (seconds >= BETSERVER_BET_PERIOD)
    {
      printf("%.1f sec passed\n", seconds);
      previous_time = current_time;
      if (count_betting_clients() > 0)
      {
        /* get winner */
        unsigned int winner = roll_the_dice();
        printf("winner is %u\n", winner);

        /* send winner around */
        handle_winner(winner, listen_socket, &sockets, sockets_size);
      }
    }
    
    int selected = check_updated_sockets(&sockets, &sockets_updated, sockets_size);
    if (selected == 0) continue;

    /*run through the existing connections looking for data to be read*/
    handle_updated_sockets(&sockets, &sockets_updated, &sockets_size, listen_socket);
  }
  
  /* Clean up */
  close(listen_socket);
  puts("Betserver stopped.");

  return 0;
}
