// -*-c++-*-
/* $Id$ */

#include "ok.h"
#include "cgi.h"
#include "pub.h"
#include <unistd.h>

class oksrvc_google_t : public oksrvc_t {
public:
  oksrvc_google_t (int argc, char *argv[]) : oksrvc_t (argc, argv) {}
  okclnt_t *make_newclnt (ptr<ahttpcon> x);
  void init_publist () { /*o init_publist (); o*/ }
};

class okclnt_google_t : public okclnt_t {
public:
  okclnt_google_t (ptr<ahttpcon> x, oksrvc_google_t *o) 
      : okclnt_t (x, o), ok_google (o) {}
  ~okclnt_google_t () {}
  void process ()
  {
    redirect ("http://www.google.com");
  }
  oksrvc_google_t *ok_google;
};

okclnt_t *
oksrvc_google_t::make_newclnt (ptr<ahttpcon> x)
{ 
  return New okclnt_google_t (x, this); 
}

int
main (int argc, char *argv[])
{
  oksrvc_t *oksrvc = New oksrvc_google_t (argc, argv);
  oksrvc->launch ();
  amain ();
}
