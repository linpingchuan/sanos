//
// udpsock.c
//
// Copyright (c) 2001 Michael Ringgaard. All rights reserved.
//
// UDP socket interface
//

#include <net/net.h>

static err_t recv_udp(void *arg, struct udp_pcb *pcb, struct pbuf *p, struct ip_addr *addr, unsigned short port)
{
  struct socket *s = arg;
  struct sockreq *req = s->waithead;
  struct sockaddr_in *sin;
  int rc;

  if (req)
  {
    rc = write_iovec(req->msg->iov, req->msg->iovlen, p->payload, p->len);
    if (rc < p->len) rc = -EMSGSIZE;

    if (req->msg->name)
    {
      if (req->msg->namelen < sizeof(struct sockaddr_in))
	rc = -EFAULT;
      else
      {
	sin = (struct sockaddr_in *) req->msg->name;
	sin->sin_len = sizeof(struct sockaddr_in);
	sin->sin_family = AF_INET;
	sin->sin_port = htons(port);
	sin->sin_addr.s_addr = addr->addr;
      }
    }
    req->msg->namelen = sizeof(struct sockaddr_in);

    pbuf_free(p);

    release_socket_request(req, rc);
  }
  else 
  {
    if (p->next) 
    {
      kprintf("recv_udp: fragmented pbuf not supported\n");
      return -EINVAL;
    }

    if (s->udp.recvtail)
    {
      pbuf_chain(s->udp.recvtail, p);
      s->udp.recvtail = p;
    }
    else
    {
      s->udp.recvhead = p;
      s->udp.recvtail = p;
    }
  }

  return 0;
}

static int udpsock_accept(struct socket *s, struct sockaddr *addr, int *addrlen, struct socket **retval)
{
  return -EINVAL;
}

static int udpsock_bind(struct socket *s, struct sockaddr *name, int namelen)
{
  int rc;
  struct sockaddr_in *sin;

  if (!name) return -EINVAL;
  if (namelen < sizeof(struct sockaddr_in)) return -EINVAL;
  sin = (struct sockaddr_in *) name;
  if (sin->sin_family != AF_INET && sin->sin_family != AF_UNSPEC) return -EPROTONOSUPPORT;

  if (!s->udp.pcb)
  {
    s->udp.pcb = udp_new();
    if (!s->udp.pcb) return -ENOMEM;
    udp_recv(s->udp.pcb, recv_udp, s);
  }

  rc = udp_bind(s->udp.pcb, (struct ip_addr *) &sin->sin_addr, ntohs(sin->sin_port));
  if (rc < 0) return rc;

  s->state = SOCKSTATE_BOUND;
  return 0;
}

static int udpsock_close(struct socket *s)
{
  struct sockreq *req;
  struct sockreq *next;

  s->state = SOCKSTATE_CLOSED;
  req = s->waithead;
  while (req)
  {
    next = req->next;
    release_socket_request(req, -EABORT);
    req = next;
  }

  if (s->udp.pcb)
  {
    s->udp.pcb->recv_arg = NULL;
    udp_remove(s->udp.pcb);
  }

  if (s->udp.recvhead) pbuf_free(s->udp.recvhead);

  return 0;
}

static int udpsock_connect(struct socket *s, struct sockaddr *name, int namelen)
{
  int rc;
  struct sockaddr_in *sin;

  if (!name) return -EINVAL;
  if (namelen < sizeof(struct sockaddr_in)) return -EINVAL;
  sin = (struct sockaddr_in *) name;
  if (sin->sin_family != AF_INET && sin->sin_family != AF_UNSPEC) return -EPROTONOSUPPORT;
  if (s->state == SOCKSTATE_CLOSED) return -EINVAL;

  if (!s->udp.pcb)
  {
    s->udp.pcb = udp_new();
    if (!s->udp.pcb) return -ENOMEM;
    udp_recv(s->udp.pcb, recv_udp, s);
  }

  rc = udp_connect(s->udp.pcb, (struct ip_addr *) &sin->sin_addr, ntohs(sin->sin_port));
  if (rc < 0) return rc;

  s->state = SOCKSTATE_CONNECTED;
  return 0;
}

static int udpsock_getpeername(struct socket *s, struct sockaddr *name, int *namelen)
{
  struct sockaddr_in *sin;

  if (!namelen) return -EINVAL;
  if (*namelen < sizeof(struct sockaddr_in)) return -EINVAL;
  if (s->state != SOCKSTATE_CONNECTED) return -EINVAL;

  sin = (struct sockaddr_in *) name;
  sin->sin_len = sizeof(struct sockaddr_in);
  sin->sin_family = AF_INET;
  sin->sin_port = htons(s->udp.pcb->remote_port);
  sin->sin_addr.s_addr = s->udp.pcb->remote_ip.addr;

  *namelen = sizeof(struct sockaddr_in);
  return 0;
}

static int udpsock_getsockname(struct socket *s, struct sockaddr *name, int *namelen)
{
  struct sockaddr_in *sin;

  if (!namelen) return -EINVAL;
  if (*namelen < sizeof(struct sockaddr_in)) return -EINVAL;
  if (s->state != SOCKSTATE_CONNECTED) return -EINVAL;

  sin = (struct sockaddr_in *) name;
  sin->sin_len = sizeof(struct sockaddr_in);
  sin->sin_family = AF_INET;
  sin->sin_port = htons(s->udp.pcb->local_port);
  sin->sin_addr.s_addr = s->udp.pcb->local_ip.addr;

  *namelen = sizeof(struct sockaddr_in);
  return 0;
}

static int udpsock_getsockopt(struct socket *s, int level, int optname, char *optval, int *optlen)
{
  return -ENOSYS;
}

static int udpsock_ioctl(struct socket *s, int cmd, void *data, size_t size)
{
  return -ENOSYS;
}

static int udpsock_listen(struct socket *s, int backlog)
{
  return -EINVAL;
}

static int udpsock_recvmsg(struct socket *s, struct msghdr *msg, unsigned int flags)
{
  struct pbuf *p;
  struct udp_hdr *udphdr;
  struct ip_hdr *iphdr;
  void *buf;
  int len;
  int rc;
  struct sockaddr_in *sin;
  struct sockreq req;

  p = s->udp.recvhead;
  if (p)
  {
    s->udp.recvhead = pbuf_dechain(p);
    if (!s->udp.recvhead) s->udp.recvtail = NULL; 

    buf = p->payload;
    len = p->len;

    udphdr = p->payload;
    pbuf_header(p, UDP_HLEN);

    //FIXME: this does not work if there are options in the ip header
    pbuf_header(p, IP_HLEN); 
    iphdr = p->payload;

    rc = write_iovec(msg->iov, msg->iovlen, buf, len);
    if (rc < len) rc = -EMSGSIZE;

    if (msg->name)
    {
      if (msg->namelen < sizeof(struct sockaddr_in)) return -EFAULT;
      sin = (struct sockaddr_in *) msg->name;
      sin->sin_len = sizeof(struct sockaddr_in);
      sin->sin_family = AF_INET;
      sin->sin_port = htons(udphdr->src);
      sin->sin_addr.s_addr = iphdr->src.addr;
    }
    msg->namelen = sizeof(struct sockaddr_in);

    pbuf_free(p);
  }
  else
    rc = submit_socket_request(s, &req, SOCKREQ_RECV, msg, s->udp.rcvtimeo);

  return rc;
}

static int udpsock_sendmsg(struct socket *s, struct msghdr *msg, unsigned int flags)
{
  struct pbuf *p;
  int size;
  int rc;

  size = get_iovec_size(msg->iov, msg->iovlen);

  if (msg->name)
  {
    rc = udpsock_connect(s, msg->name, msg->namelen);
    if (rc < 0) return rc;
  }

  if (s->state != SOCKSTATE_CONNECTED) return -EINVAL;

  p = pbuf_alloc(PBUF_TRANSPORT, size, PBUF_RW);
  if (!p) return -ENOMEM;

  rc = read_iovec(msg->iov, msg->iovlen, p->payload, size);
  if (rc < 0) return rc;
  
  rc = udp_send(s->udp.pcb, p, NULL);
  if (rc < 0)
  {
    pbuf_free(p);
    return rc;
  }

  return size;
}

static int udpsock_setsockopt(struct socket *s, int level, int optname, const char *optval, int optlen)
{
  if (level == SOL_SOCKET)
  {
    switch (optname)
    {
      case SO_SNDTIMEO:
	if (optlen != 4) return -EINVAL;
	s->udp.sndtimeo = *(unsigned int *) optval;
	break;

      case SO_RCVTIMEO:
	if (optlen != 4) return -EINVAL;
	s->udp.rcvtimeo = *(unsigned int *) optval;
	break;

      default:
        return -EINVAL;
    }
  }
  else
    return -EINVAL;

  return 0;
}

static int udpsock_shutdown(struct socket *s, int how)
{
  return -ENOSYS;
}

static int udpsock_socket(struct socket *s, int domain, int type, int protocol)
{
  s->udp.sndtimeo = INFINITE;
  s->udp.rcvtimeo = INFINITE;
  return 0;
}

struct sockops udpops =
{
  udpsock_accept,
  udpsock_bind,
  udpsock_close,
  udpsock_connect,
  udpsock_getpeername,
  udpsock_getsockname,
  udpsock_getsockopt,
  udpsock_ioctl,
  udpsock_listen,
  udpsock_recvmsg,
  udpsock_sendmsg,
  udpsock_setsockopt,
  udpsock_shutdown,
  udpsock_socket,
};
