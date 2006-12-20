#include "uidplus_sender.h"

#include "mailimap_sender.h"

int
mailimap_uid_expunge_send(mailstream * fd,
    struct mailimap_set * set)
{
  int r;
  
  r = mailimap_token_send(fd, "UID");
  if (r != MAILIMAP_NO_ERROR)
    return r;
  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  r = mailimap_expunge_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  r = mailimap_set_send(fd, set);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  return MAILIMAP_NO_ERROR;
}
