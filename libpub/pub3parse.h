// -*-c++-*-
/* $Id: parr.h 2784 2007-04-20 16:32:00Z max $ */

#ifndef _LIBPUB_PUB3PARSE_H_
#define _LIBPUB_PUB3PARSE_H_

#include "pub.h"
#include "parr.h"
#include "pub3expr.h"
#include "okformat.h"
#include "pub3expr.h"
#include "pub3obj.h"

namespace pub3 {

  //-----------------------------------------------------------------------
  
  typedef int lineno_t;

  //-----------------------------------------------------------------------

  struct location_t {
    location_t (str f, lineno_t l) : _filename (f), _lineno (l) {}
    str _filename;
    lineno_t _lineno;
  };
  
  //-----------------------------------------------------------------------

  class parser_t {
  public:
    parser_t (str f);
    lineno_t lineno () const;
    const location_t &location () const;
    void inc_lineno (lineno_t i = 1);

    static ptr<parser_t> current ();
    static void set_current (ptr<parser_t> p);

    // callbacks from bison
    virtual bool set_zone_output (ptr<pub3::zone_t> z) { return false; }
    virtual bool set_expr_output (ptr<pub3::expr_t> x) { return false; }

  protected:
    location_t _location;
  };

  //-----------------------------------------------------------------------

  class json_parser_t : public parser_t {
  public:
    json_parser_t ();
    void set_output (ptr<expr_t> e);
    ptr<expr_t> parse (const str &in);
    bool set_expr_output (ptr<pub3::expr_t> x);
  protected:
    ptr<pub3::expr_t> _out;
  };

  //-----------------------------------------------------------------------

  class pub_parser_t : public parser_t {
  public:
    pub_parser_t (str f) : parser_t (f) {}
    void set_output (ptr<zones_t> z);
    ptr<zones_t> parse ();
    bool set_zone_output (ptr<pub3::zone_t> z);
  private:
    ptr<zone_t> _out;
  };

  //-----------------------------------------------------------------------

};


#endif /* _LIBPUB_PUB3OBJ_H_ */

