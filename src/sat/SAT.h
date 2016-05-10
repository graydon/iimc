/* -*- C++ -*- */

/********************************************************************
Copyright (c) 2010-2012, Regents of the University of Colorado

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

Neither the name of the University of Colorado nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
********************************************************************/

#ifndef _SAT_
#define _SAT_

/** @file SAT.h */

#include "Expr.h"
#include "ExprUtil.h"

#include <unordered_map>
#include <vector>

#ifdef USE_MINISAT
#include "minisat22/mcore/Solver.h"
#else
#include "zchaff/SAT_C.h"
#endif

#ifdef MTHREADS
#include "tbb/mutex.h"
#include "tbb/spin_mutex.h"
#endif

#define MAX_LITS (16*1024)

/** Namespace of SAT. */
namespace SAT {

  typedef Expr::IDVector Clause;
  typedef std::vector<Expr::IDVector> Clauses;
#ifdef USE_MINISAT
  typedef Minisat::Lit GID;
  const GID NULL_GID = Minisat::lit_Undef;
#else
  typedef unsigned int GID;
  const GID NULL_GID = 0;
#endif

  class Trivial {
  public:
    Trivial(bool v) : v(v) {}
    bool value() { return v; }
  private:
    bool v;
  };

  /**
   * Defines an assigment, a mapping from variables to AssignVals.
   */
  enum AssignVal {False, True, Unknown};
  typedef std::unordered_map<ID, AssignVal> Assignment;

  /**
   * A thread-safe SAT manager.
   */
  class Manager {
  public:
    /**
     * Creates a Manager over the given Expr::Manager.
     */
    Manager(Expr::Manager & exprMan, bool track_time = false);
    /**
     * Destroys the manager.
     */
    ~Manager();

    /**
     * Adds a clause (permanently) to the global clause database.
     * Returns false if the clause is equivalent to false.
     */
    bool add(Clause & clause) throw(Trivial);

    /**
     * Adds a list of clauses (permanently) to the global clause
     * database.  Returns false if any clause is equivalent to false.
     */
    bool add(Clauses & clauses) throw(Trivial);

    class View;
    /**
     * Creates a thread-local view of the SAT solver over the given
     * Expr::Manager::View.
     */
    View * newView(Expr::Manager::View & exprView);

    static uint64_t satTime() { return sat_time; }
    static unsigned int satCount() { return sat_count; }

    /**
     * A thread-local view of the SAT solver.
     */
    class View {
      friend class Manager;
    public:
      /**
       * Deletes the view.
       */
      ~View();

      /**
       * Adds a clause to the thread-local clause database with the
       * given Group ID.  Returns false if the clause is equivalent to
       * false.
       */
      bool add(Clause & clause, GID gid = NULL_GID) throw(Trivial);
      /**
       * Adds a list of clauses to the thread-local clause database
       * with the given Group ID.  Returns false if any clause is
       * equivalent to false.
       */
      bool add(Clauses & clauses, GID gid = NULL_GID) throw(Trivial);

      /**
       * Allocates a new Group ID.
       */
      GID newGID();
      /**
       * Removes all clauses with the given Group ID.
       */
      void remove(GID gid);

      /**
       * Solves the current SAT problem using the global database, the
       * thread-local database, and the list of given literal
       * assumptions.  If the instance is satisifable, it returns
       * true; and if asgn != NULL, the assignment is stored in the
       * given map.  If the instance is unsatisfiable, it returns
       * false; and if criticalUnits != NULL, the literals in
       * criticalUnits that are used in the proof of unsatisfiability
       * are kept, while all others are removed.  If the same IDVector
       * is used as both assumptions and criticalUnits and the
       * instance is unsatisfiable, the vector contains only the
       * "necessary" literals after the method returns.  [The GID, if
       * provided, is used for handling assumptions in the ZChaff
       * implementation; it can only be used when there is when sat()
       * call involving the clause group.
       */
      bool sat(Expr::IDVector * assumptions = NULL, 
               Assignment * asgn = NULL, 
               Expr::IDVector * criticalUnits = NULL, 
               GID gid = NULL_GID,
               bool full_init = false,
               Assignment * lift = NULL);

      /**
       * Returns this view's manager.
       */
      Manager & manager() { return man; }

      /**
       * Set this view's maximum allotted solving time.
       */
      void timeout(double to = -1.0);

    private:
      View(Manager &, Expr::Manager::View &);

      Manager & man;
      Expr::Manager::View & exprView;

#ifdef MTHREADS
      typedef tbb::spin_mutex VMuxType;
      VMuxType mux;
#endif

#ifdef USE_MINISAT
      typedef std::unordered_map<ID, Minisat::Var> VMap;
      typedef std::unordered_map<Minisat::Var, ID> IVMap;
#else
      typedef std::unordered_map<ID, int> VMap;
      typedef std::unordered_map<int, ID> IVMap;
#endif
      VMap vmap;
      IVMap ivmap;

#ifdef USE_MINISAT
      Minisat::Solver satMan;
      std::set<Minisat::Lit> _assumps;
#else
      SAT_Manager satMan;
      int _lits[MAX_LITS];
#endif
    };

  private:
    friend class View;

    Expr::Manager & exprMan;

#ifdef MTHREADS
    typedef tbb::mutex MMuxType;
    MMuxType mux;
#endif

    std::vector<View *> views;
    Clauses clauses;

    bool tt;
    static uint64_t sat_time;
    static unsigned int sat_count;
  };

}

#endif
