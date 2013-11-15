// Copyright (C) 2013 Vrije Universiteit Brussel, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.


#ifndef cf3_lss_linearsystem_hpp
#define cf3_lss_linearsystem_hpp


#include "common/BasicExceptions.hpp"
#include "common/Signal.hpp"
#include "common/Action.hpp"

#include "utilities.hpp"
#include "matrix.hpp"
#include "index.hpp"


namespace cf3 {
namespace lss {


/* -- linear system --------------------------------------------------------- */


// helper forward declarations
template< typename T > class linearsystem;
template< typename T > std::ostream& operator<< (std::ostream&, const linearsystem< T >&);


/**
 * @brief Description of a linear system (suitable for sparse matrix solvers.)
 */
template< typename T >
class linearsystem : public common::Action
{
 protected:
  // utility definitions
  typedef dense_matrix_v< T, sort_by_column > vector_t;

  // -- Construction and destruction
 public:

  /// Construct the linear system
  linearsystem(const std::string& name) :
    common::Action(name),
    m_dummy_value(std::numeric_limits< T >::quiet_NaN())
  {
    // framework scripting: options level, signals and options
    mark_basic();

    regist_signal("initialize")
        .description("Initialize with given size (i,j,k) and value (value), or from file (A, b, and/or x)")
        .connect   ( boost::bind( &linearsystem::signal_initialize, this, _1 ))
        .signature ( boost::bind( &linearsystem::signat_ijkvalue,   this, _1 ));

    regist_signal("zerorow")
        .description("Erase given row (i) in all components")
        .connect   ( boost::bind( &linearsystem::signal_zerorow,  this, _1 ))
        .signature ( boost::bind( &linearsystem::signat_ijkvalue, this, _1 ));

    regist_signal("output")
        .description("Print a pretty linear system, at print level per component where 0:auto (default), 1:size, 2:signs, and 3:full")
        .connect   ( boost::bind( &linearsystem::signal_output,  this, _1 ))
        .signature ( boost::bind( &linearsystem::signat_abcfile, this, _1 ));

    regist_signal("clear") .connect( boost::bind( &linearsystem::signal_clear,  this )).description("Empty linear system components");
    regist_signal("solve") .connect( boost::bind( &linearsystem::signal_solve,  this )).description("Solve linear system, returning solution in x");

    regist_signal("A").connect(boost::bind( &linearsystem::signal_A, this, _1 )).signature(boost::bind( &linearsystem::signat_ijkvalue, this, _1 )).description("Set entry in matrix A, by given index (i,j) and value (value)");
    regist_signal("b").connect(boost::bind( &linearsystem::signal_b, this, _1 )).signature(boost::bind( &linearsystem::signat_ijkvalue, this, _1 )).description("Set entry in vector b, by given index (i,k) and value (value)");
    regist_signal("x").connect(boost::bind( &linearsystem::signal_x, this, _1 )).signature(boost::bind( &linearsystem::signat_ijkvalue, this, _1 )).description("Set entry in vector x, by given index (j,k) and value (value)");

    options().add("A",std::vector< double >())
        .link_to(&m_dummy_vector).mark_basic()
        .attach_trigger(boost::bind( &linearsystem::trigger_A, this ));

    options().add("b",std::vector< double >())
        .link_to(&m_dummy_vector).mark_basic()
        .attach_trigger(boost::bind( &linearsystem::trigger_b, this ));

    options().add("x",std::vector< double >())
        .link_to(&m_dummy_vector).mark_basic()
        .attach_trigger(boost::bind( &linearsystem::trigger_x, this ));

    /*
     * note: you cannot call any pure methods (or any method that would call a
     * pure one) in the constructor! specifically, initialize() can't be called
     * here, so the child constructor has to call initialize() itself.
     */
  }

  /// Destruct the linear system
  virtual ~linearsystem() {}


  // -- Framework scripting
 private:


  void signat_ijkvalue(common::SignalArgs& args) {
    common::XML::SignalOptions opts(args);
    opts.add< unsigned >("i");
    opts.add< unsigned >("j");
    opts.add< unsigned >("k",1);
    opts.add< std::string >("A");
    opts.add< std::string >("b");
    opts.add< std::string >("x");
    opts.add< double >("value");
  }

  void signat_abcfile(common::SignalArgs& args) {
    common::XML::SignalOptions opts(args);
    opts.add< int >("A",(int) print_auto);
    opts.add< int >("b",(int) print_auto);
    opts.add< int >("x",(int) print_auto);
    opts.add< std::string >("file","");
  }

  void signal_initialize(common::SignalArgs& args) {
    common::XML::SignalOptions opts(args);
    const std::string
        Afname(opts.value< std::string >("A")),
        bfname(opts.value< std::string >("b")),
        xfname(opts.value< std::string >("x"));
    const double value(opts.value< double >("value"));
    if (Afname.length() || bfname.length() || xfname.length()) {
      if (Afname.length()) A___initialize(Afname);
      if (bfname.length() && !component_initialize_with_file(m_b,"b",bfname) || !bfname.length()) m_b.initialize(size(0),1,      value);
      if (xfname.length() && !component_initialize_with_file(m_x,"x",xfname) || !xfname.length()) m_x.initialize(size(1),size(2),value);
      consistent(A___size(0),A___size(1),m_b.size(0),m_b.size(1),m_x.size(0),m_x.size(1));
    }
    else {
      const unsigned
          i(opts.value< unsigned >("i")),
          j(opts.value< unsigned >("j")),
          k(opts.value< unsigned >("k"));
      A___initialize(i,j,value);
      m_b.initialize(i,k,value);
      m_x.initialize(j,k,value);
    }
  }


  void signal_zerorow(common::SignalArgs& args) {
    common::XML::SignalOptions opts(args);
    zerorow(opts.value< unsigned >("i"));
  }

  void signal_output(common::SignalArgs& args) {
    common::XML::SignalOptions opts(args);
    using namespace std;

    string file = opts.value< string >("file");
    if (file.length()) {
      A___print_level(print_file);
      m_b.m_print = print_file;
      m_x.m_print = print_file;
      try {
        ofstream f;
        string fname;

        f.open((fname=(file+"_A.mtx")).c_str());
        if (!f) throw runtime_error("A: cannot write to file \""+fname+"\"");
        A___print(f);
        f.close();

        f.open((fname=(file+"_b.mtx")).c_str());
        if (!f) throw runtime_error("b: cannot write to file \""+fname+"\"");
        m_b.print(f);
        f.close();

        f.open((fname=(file+"_x.mtx")).c_str());
        if (!f) throw runtime_error("x: cannot write to file \""+fname+"\"");
        m_x.print(f);
        f.close();
      }
      catch (const std::runtime_error& e) {
        CFwarn << "linearsystem: " << e.what() << CFendl;
      }
    }
    else {
      A___print_level(print_level(opts.value< int >("A")));
      m_b.m_print =   print_level(opts.value< int >("b"));
      m_x.m_print =   print_level(opts.value< int >("x"));
      operator<<(cout,*this);
    }
    A___print_level(print_auto);
    m_b.m_print =   print_auto;
    m_x.m_print =   print_auto;
  }

  void signal_clear () { clear(); }
  void signal_solve () { execute(); }

  void signal_A(common::SignalArgs& args) { common::XML::SignalOptions opts(args); A  (opts.value< unsigned >("i"),opts.value< unsigned >("j")) = opts.value< double   >("value"); }
  void signal_b(common::SignalArgs& args) { common::XML::SignalOptions opts(args); m_b(opts.value< unsigned >("i"),opts.value< unsigned >("k")) = opts.value< unsigned >("value"); }
  void signal_x(common::SignalArgs& args) { common::XML::SignalOptions opts(args); m_x(opts.value< unsigned >("j"),opts.value< unsigned >("k")) = opts.value< unsigned >("value"); }

  void trigger_A() { try { m_dummy_vector.size()==1? A___initialize(size(0),size(1),m_dummy_vector[0]) : A___initialize(m_dummy_vector); } catch (const std::runtime_error& e) { CFwarn << "linearsystem: A: " << e.what() << CFendl; } m_dummy_vector.clear(); }
  void trigger_b() { try { m_dummy_vector.size()==1? m_b.initialize(size(0),size(2),m_dummy_vector[0]) : m_b.initialize(m_dummy_vector); } catch (const std::runtime_error& e) { CFwarn << "linearsystem: b: " << e.what() << CFendl; } m_dummy_vector.clear();; }
  void trigger_x() { try { m_dummy_vector.size()==1? m_x.initialize(size(1),size(2),m_dummy_vector[0]) : m_x.initialize(m_dummy_vector); } catch (const std::runtime_error& e) { CFwarn << "linearsystem: x: " << e.what() << CFendl; } m_dummy_vector.clear();; }

  bool component_initialize_with_file(dense_matrix_v< T >& c, const std::string& name, const std::string& fname) {
    try { c.initialize(fname); }
    catch (const std::runtime_error& e) {
      CFwarn << "linearsystem: " << name << ": " << e.what() << CFendl;
      return false;
    }
    m_dummy_vector.clear();
    return true;
  }


  // -- Basic functionality
 public:

  /// Linear system solving, aliased from execute
  void execute() {
    try { solve(); }
    catch (const std::runtime_error& e) {
      std::cout << "linearsystem: " << e.what() << std::endl;
    }
  }

  /// Initialize the linear system
  virtual linearsystem& initialize(
      const size_t& i=size_t(),
      const size_t& j=size_t(),
      const size_t& k=1,
      const double& _value=double())
  {
    A___initialize(i,j,_value);
    m_b.initialize(i,k,_value);
    m_x.initialize(j,k,_value);
    return *this;
  }

  /// Initialize linear system from file(s)
  virtual linearsystem& initialize(
      const std::string& _Afname,
      const std::string& _bfname="",
      const std::string& _xfname="" ) {
    if (_Afname.length()) A___initialize(_Afname);
    if (_bfname.length()) m_b.initialize(_bfname); else m_b.initialize(size(0),1);
    if (_xfname.length()) m_x.initialize(_xfname); else m_x.initialize(size(1),size(2));
    consistent(A___size(0),A___size(1),m_b.size(0),m_b.size(1),m_x.size(0),m_x.size(1));
    return *this;
  }

  /// Initialize linear system from vectors of values (lists, in the right context)
  virtual linearsystem& initialize(
      const std::vector< double >& vA,
      const std::vector< double >& vb=std::vector< double >(),
      const std::vector< double >& vx=std::vector< double >()) {
    if (vA.size()) A___initialize(vA);
    if (vb.size()) m_b.initialize(vb); else m_b.initialize(size(0),1);
    if (vx.size()) m_x.initialize(vx); else m_x.initialize(size(1),size(2));
    consistent(A___size(0),A___size(1),m_b.size(0),m_b.size(1),m_x.size(0),m_x.size(1));
    return *this;
  }

  virtual linearsystem& initialize(const index_t& _index) {
    A___initialize(_index);
    m_b.initialize(size(0),1);
    m_x.initialize(size(1),size(2));
    consistent(A___size(0),A___size(1),m_b.size(0),m_b.size(1),m_x.size(0),m_x.size(1));
    return *this;
  }

  /// Clear contents in all system components
  virtual linearsystem& clear() {
    A___clear();
    m_b.clear();
    m_x.clear();
    return *this;
  }

  /// Zero row in all system components
  virtual linearsystem& zerorow(const size_t& i) {
    A___zerorow(i);
    m_b.zerorow(i);
    m_x.zerorow(i);
    return *this;
  }

  /// Returns the specific dimension of the system
  size_t size(const size_t& d) const {
    return (d< 2? A___size(d) : (d==2? m_b.size(1) : 0));
  }

  /// Checks whether the linear system matrix is empty
  bool empty() { return !(size(0)*size(1)*size(2)); }



  // -- Internal functionality
 private:

  /// Checks whether the matrix/vectors sizes are consistent in the system
  bool consistent(const size_t& Ai, const size_t& Aj,
                  const size_t& bi, const size_t& bj,
                  const size_t& xi, const size_t& xj) const {
    if (Ai!=bi || Aj!=xi || bj!=xj) {
      std::ostringstream msg;
      msg << "linearsystem: size is not consistent: "
          << "A(" << Ai << 'x' << Aj << ") "
          << "x(" << xi << 'x' << xj << ") = "
          << "b(" << bi << 'x' << bj << ").";
      throw std::runtime_error(msg.str());
      return false;
    }
    return true;
  }

  /// Output
  template< typename aT >
  friend std::ostream& operator<< (std::ostream&, const linearsystem< aT >&);
  // (a private generic friend? how promiscuous!)


  // -- Storage
 protected:

  /// Linear system components: b and x vectors
  vector_t m_b;
  vector_t m_x;

  /// Scripting temporary storage
  T                     m_dummy_value;
  std::vector< double > m_dummy_vector;


  // -- Indexing (absolute)
 public:

  virtual const T& A(const size_t& i, const size_t& j)   const = 0;
  virtual       T& A(const size_t& i, const size_t& j)         = 0;
          const T& b(const size_t& i, const size_t& j=0) const { return m_b(i,j); }
          const T& x(const size_t& i, const size_t& j=0) const { return m_x(i,j); }
                T& b(const size_t& i, const size_t& j=0)       { return m_b(i,j); }
                T& x(const size_t& i, const size_t& j=0)       { return m_x(i,j); }


  // -- Interfacing
 public:

  /// Linear system solving
  virtual linearsystem& solve() = 0;

  /// Linear system matrix modifiers
  virtual void A___initialize(const size_t& i, const size_t& j, const double& _value=double()) = 0;
  virtual void A___initialize(const std::vector< double >& _vector) = 0;
  virtual void A___initialize(const std::string& _fname)            = 0;
  virtual void A___initialize(const index_t& _index)                = 0;
  virtual void A___clear()                    = 0;
  virtual void A___zerorow(const size_t& i)   = 0;
  virtual void A___print_level(const int& _l) = 0;

  /// Linear system matrix inspecting
  virtual std::ostream& A___print(std::ostream& o) const = 0;
  virtual size_t        A___size(const size_t& d ) const = 0;


#if 0
  /// Linear system copy
  linearsystem& operator=(const linearsystem& _other) {
    A() = _other.A();
    m_b = _other.m_b;
    m_x = _other.m_x;
    return *this;
  }

  /// Value assignment (operator)
  linearsystem& operator=(const T& _value) { return initialize(size(0),size(1),size(2),_value); }

  /// Value assignment (method)
  linearsystem& assign(const T& _value=T()) { return operator=(_value); }
#endif

};


/// Output to given std::ostream (non-member version)
template< typename T >
std::ostream& operator<< (std::ostream& o, const linearsystem< T >& lss)
{
  o << "linearsystem: A: "; lss.A___print(o); o << std::endl;
  o << "linearsystem: b: "; lss.m_b.print(o); o << std::endl;
  o << "linearsystem: x: "; lss.m_x.print(o); o << std::endl;
  return o;
}


}  // namespace lss
}  // namespace cf3


#endif
