// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
///////////////////////////////////////////////////////////////////////////////
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2004, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: cdd.h,v 1.13 2004/08/18 09:25:43 behrmann Exp $
//
///////////////////////////////////////////////////////////////////////////////

#include "cdd/config.h"
#ifndef CDD_CDD_H
#define CDD_CDD_H

/**
 * @mainpage
 *
 * The CDD module is an implementation of Clock Difference Diagrams
 * and Boolean Decision Diagrams. The module is written in C, but also
 * provides a C++ wrapper. Please consult the modules page for
 * information about each interface.
 *
 * @section cdd Clock Difference Diagrams
 * In recent years, several approaches to fully symbolic model
 * checking of timed automata have been investigated.  Most of these
 * try to apply ROBDDs or similar data structures and thus copy the
 * approach which has proven to be so successful in the untimed
 * case. E.g. Rabbit [bln:cav03] uses a discrete time interpretation
 * of timed automata and can thus encode the transition relation
 * directly using ROBDDs.  NDDs [ndd] are regular BDDs used to encode
 * the region graph of a dense-time timed automaton. CDDs [blpww99],
 * DDDs [ddd-smc99] and CDRs [wang01:cdr] are all variations of the
 * same idea (discovered at roughly the same time by several research
 * groups around the world).
 *
 * Structurally, a CDD is very similar to a BDD. The two main
 * differences are that:
 *
 * - The nodes of the DAG are labelled with pairs of clocks which are
 *   interpreted as clock differences, rather than boolean variables.
 *   I.e. elements \f$(i, j)\f$ in \f$X\times X\f$, where \f$X\f$ is a
 *   set of clocks, such that \f$i < j\f$ according to some ordering
 *   of the clocks.
 * - Any intermediate node can have two or more out-edges, each
 *   labelled with a non-empty, convex, integer-bounded subset of the
 *   real line, which together form a partitioning of \f$R\f$.
 *
 * Like for BDDs, we say that a CDD is ordered if there exist a total
 * order of \f$X \times X\f$ such that all paths from the
 * root to a terminal respect this ordering. A CDD is reduced if:
 *
 * - There are no duplicate nodes.
 * - There are no trivial edges, i.e. nodes with only one outgoing
 *   edge.
 * - The intervals on the edges are maximal, i.e. for two edges
 *   between the same two nodes, either the intervals are the same
 *   or the union is not an interval.
 *
 * Semantically, we might view a CDD as a function \f$f: R^X
 * \rightarrow B\f$, which could be the characteristic function of a
 * set of clock valuations. Contrary to DBMs (which are limited to
 * convex unions of regions), CDDs can represent arbitrary unions of
 * regions.  Implementing common operations like negation, set union,
 * set intersection, etc. is easy and can be achieved in time
 * polynomial to the size of the argument CDDs.  Contrary to ROBDDs, a
 * reduced and ordered CDD is not a canonical representation. The
 * literature contains a few conjectures of normal forms for CDDs and
 * DDDs, but these are very expensive to compute and do not have
 * practical value. A CDD can be brought into a semi-canonical form by
 * eliminating infeasible paths (paths for which there are no clock
 * valuations satisfying all constraints of the path) from the CDD.
 * In this form, a tautology is uniquely represented by the \b true
 * terminal and an unsatisfiable function by the \b false terminal. By
 * combining CDDs and BDDs into one data structure, one can perform
 * fully symbolic model checking of timed automata.  Unfortunately,
 * existential quantification takes exponential time in the size of
 * the CDD (whereas it is polynomial for ROBDDs). Existential
 * quantification is essential for computing the \b future of a CDD
 * (i.e. delay transitions).
 *
 * Notice one important difference between DBMs and CDDs: DBMs always
 * have a special zero-clock, which is used for encoding constrains on
 * individual clocks. The same encoding is used in CDDs, but there is
 * no fixed clock reserved for this purpose. This also means that CDDs
 * can encode clock valuations with negative entries.
 *
 * @section library What is in the module
 *
 * The CDD module supports two types of nodes: (RO)BDD nodes and CDD
 * nodes. If you do not define any clocks, the library acts as a
 * simple BDD library (simple because is has fewer features than most
 * other BDD libraries).
 *
 * When using both BDD and CDD nodes, you end up having more types:
 * - There is one type for each boolean variable.
 * - There is a type for each pair \f$(i,j)\f$ in \f$X \times (X \cup
 *   \{x_0\})\f$, where \f$j < i\f$.
 *
 * In the library, each type has a \b level. Level indicates the position
 * or order on the path from the root to the terminal nodes.
 *
 * @subsection Initialisation
 *
 * Before use, the library must be initialised by calling \c
 * cdd_init(), boolean variables must be added by calling \c
 * cdd_add_bddvar(), and clock variables must be added by calling \c
 * cdd_add_clocks().
 *
 * After use, all resources allocated by the library can be
 * released by calling \c cdd_done().
 *
 * @subsection operations Operations
 *
 * The library only supports a limited number of operations on
 * decision diagrams. These include the \b and
 * (conjunction), \b xor, \b negation, \b exist (existential
 * quantification), and the ternary \b if-then-else
 * operation. Variable substitution is also supported.
 *
 * A number of DBM related operations are supported, such as
 * converting a DBM to a CDD, converting a CDD to a DBM and checking
 * whether a DBM is contained in a CDD.
 *
 * Finally, it is possible to compute the reduced form of a CDD.  In
 * this form all inconsistent paths are removed from the CDD,
 * resulting in a pseudo canonical form where the empty CDD is
 * represented by \c cddfalse and the tautology is represented by \c
 * cddtrue. Notice that an unconstrained DBM is not the same as \c
 * cddtrue, since the DBM only contains positive clock valuations.
 *
 * @subsection gbc Garbage Collection
 *
 * All decision diagram nodes are internally reference counted. The
 * C++ interface automatically maintains the reference counts. In
 * the C interface the user must increment and decrement the
 * references manually.
 *
 * The library is preallocating memory for allocating decision diagram
 * nodes. When running out of free nodes, the library uses some
 * heuristics to decide whether to allocate more memory or to reclaim
 * unused nodes by invoking the garbage collector.
 *
 * There is a trade off between allocating new memory and invoking the
 * garbage collector:
 * - Allocating more memory obviously requires some memory and forces
 *   the library to rehash all nodes.
 * - Garbage collecting is an expensive operation. In case only a
 *   few nodes are freed we will soon run out of free nodes again.
 *
 * The garbage collector uses a mark-and-sweep algorithm. You can
 * invoke it manually by calling \c cdd_gbc(), but notice that this is
 * rather expensive: It takes time to run the garbage collector, and
 * even worse is that the internal operation cache is cleared on each
 * invocation. You can add hooks to the garbage collector.
 */

/**
 * @file cdd.h
 *
 * The public header file for the CDD library.
 */

#include <stdio.h>  // FILE

#include <dbm/dbm.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup cinterface The C interface
 *
 * The C interface must be used for initialisation and for hooking
 * into the garbage collector.
 *
 * It also supports operations on decision diagrams, but you have to
 * increase and decrease the reference count yourself. Maintaining the
 * reference count is error prone and we recommend using the C++
 * interface, which maintains the reference count automatically.
 *
 * @{
 *
 */

#define cdd_neg(node) ((ddNode*)((size_t)(node) ^ 0x1))

#define cddop_and 0 /**< AND operation. @see cdd_apply() */
#define cddop_xor 1 /**< XOR operation. @see cdd_apply() */

#define TYPE_CDD 0
#define TYPE_BDD 1

/** Base type for decision diagram nodes. */
typedef struct node_ ddNode;

/** Structure with information about garbage collection runs. */
typedef struct s_CddGbcStat
{
    int32_t nodes;     /**< Number of allocated nodes. */
    int32_t freenodes; /**< Number of free nodes */
    int64_t time;      /**< Time used for garbage collection */
    int64_t sumtime;   /**< Accumulated time for garbage collection */
    int32_t num;       /**< Number of times garbage collection was done */
} CddGbcStat;

/** Structure with information about rehashing */
typedef struct s_CddRehashStat
{
    int32_t level;   /**< The level of the subtable being rehashed */
    int32_t buckets; /**< New size of hash table */
    int32_t keys;    /**< Number of elements in the hash table */
    int32_t max;     /**< Max. number of elements before next rehash */
    int32_t num;     /**< How many times we have rehashed (in total) */
    int64_t time;    /**< Time used to rehash */
    int64_t sumtime; /**< Accumulated time used to rehash */
} CddRehashStat;

/** Structure with information about a level in a decision diagram */
typedef struct
{
    int32_t type;   /**< TYPE_CDD or TYPE_BDD */
    int32_t clock1; /**< Positive clock index */
    int32_t clock2; /**< Negative clock index */
    int32_t diff;   /**< Encoding of clock1 - clock2 */
} LevelInfo;

#define cdd_difference_count(n) (((n) * ((n)-1)) >> 1)
#define cdd_difference(c, d)    (cdd_difference_count(c) + (d))
//#define CDD_DIFF(c,d) ((c) << 10 | (d))
//#define CDD_CLOCK1(t) ((t) >> 10)
//#define CDD_CLOCK2(t) ((t) & 0x3FF)
//#define CDD_ISVALIDDIFF(t) (CDD_CLOCK1(t) < CDD_CLOCK2(t))

/**
 * @name Initialisation and Information
 * Functions for library initialisation and for querying the
 * status of the library.
 * @{
 */

/**
 * Initialise CDD library.
 * @param maxsize   the maximum arity of a decision diagram node.
 * @param cs        number of entries in operation cache.
 * @param stacksize size of stack used to keep temporary references.
 * @return 0 on success, or a non-zero error code on failure
 */
extern int32_t cdd_init(int32_t maxsize, int32_t cs, size_t stacksize);

/**
 * Deinitialized CDD library. This method frees all resources allocated
 * by the library.
 */
extern void cdd_done();

/**
 * Make sure the cdd library is running & initialize it
 * with default values if it's not running.
 */
extern void cdd_ensure_running();

/**
 * Declares a number of BDD variables. The library maintains a list of
 * boolean and clock variables. This function adds more BDD variable
 * to this list. Each BDD variable is identified by a \a level
 * (counting from 0).
 * @param n the number of BDD variables to add
 * @return the level of the first BDD variable added
 */
extern int32_t cdd_add_bddvar(int32_t n);

/**
 * Interpret the CDD as clock values, and remove any negative
 * clock values.
 * @param the original cdd
 * @return a cdd that does not contain negative value
 */
extern ddNode* cdd_remove_negative(ddNode* node);

/**
 * Declares a number of clock variables. The library maintains a list
 * of boolean and clock variables. This functions adds more clock
 * variables to this list. Since CDD nodes describe differences
 * between clocks, the library has to maintain \f$O(c^2)\f$ levels,
 * where c is the number of clocks.
 * @param n the number of clocks to add
 */
extern void cdd_add_clocks(int32_t n);

#ifdef MULTI_TERMINAL

/**
 * The extra terminals correspond to special end variables that
 * evaluate to true for the purpose of reducing and evaluating the
 * BDD/CDD. They are like tautologies but we want to
 * know which one will be evaluated to true instead of just "true".
 * This function should never be called after nodes containing
 * extra terminals have been created.
 */
extern void cdd_add_tautologies(int32_t n);

/**
 * @returns a new CDD = node ^ extra_tautology.
 */
extern ddNode* cdd_apply_tautology(ddNode* node, int32_t t_id);

/**
 * @return the ID of a tautology node.
 * @pre cdd_is_extra_terminal(node)
 */
extern int32_t cdd_get_tautology_id(ddNode* node);

/**
 * @return 1 if the node evaluates to cddtrue or a
 * tautology, 0 otherwise.
 */
extern int32_t cdd_eval_true(ddNode* node);

/**
 * @return 1 if the node evaluates to cddfalse or a
 * negated tautology, 0 otherwise.
 */
extern int32_t cdd_eval_false(ddNode* node);

/**
 * @return the number of tautologies.
 */
extern int32_t cdd_get_number_of_tautologies();

#endif

/**
 * Returns the number of levels. Each level identifies a BDD or CDD
 * node type. BDD nodes are used for boolean variables whereas CDD
 * nodes are used for constraint on the difference between clock
 * variables. Thus the number of levels is \f$O(n^2 + m)\f$, where n
 * is the number of clocks and m is the number of boolean variables.
 * @return the number of levels
 */
extern int32_t cdd_get_level_count();

/**
 * Checks for equivalence between two CDDs.
 */
extern int32_t cdd_equiv(ddNode* c, ddNode* d);

/**
 * Returns the number of BDD levels.
 */
extern int32_t cdd_get_bdd_level_count();

/**
 * Returns the number of declared clocks. This number is increased with
 * every call to cdd_add_clocks().
 * @return the number of declared clocks.
 */
extern int32_t cdd_getclocks();

/**
 * Returns true if the library has been initialised.
 * @return true if cdd_init() has been called one more time than cdd_done()
 */
extern int32_t cdd_isrunning();

/**
 * Returns the version identification of the library.
 * @return the version string
 */
extern const char* cdd_versionstr();

/**
 * Returns the the version number. The version number is encoded base 10
 * with the least significant digit being the minor number of the rest
 * being the major number.
 * @return the version number
 */
extern int32_t cdd_versionnum();

/**
 * Return the information structure about \a level.
 * @param level the node level for which to return information
 * @return a pointer to a \c LevelInfo structure.
 * @see LevelInfo
 */
extern const LevelInfo* cdd_get_levelinfo(int32_t level);

/**
 * Mark clock. @todo
 */
extern void cdd_mark_clock(int32_t*, int);

/** @} */

/**
 * @name Garbage collection
 * Functions for garbage collection.
 * @{
 */

/**
 * Set pre GBC hook. The hook is called before each garbage collection
 * run.
 * @param func a pointer to void function
 * @see cdd_postgbc_hook
 */
extern void cdd_pregbc_hook(void (*func)(void));

/**
 * Set post GBC hook. The hook is called after each garbage collection
 * run.
 * @param func a pointer to a function taking a CddGbcStat pointer argument
 * @see cdd_pregbc_hook
 * @see cdd_default_gbhandler
 */
extern void cdd_postgbc_hook(void (*func)(CddGbcStat*));

/**
 * Set pre rehash hook. The hook is called before each rehashing (i.e.
 * resizing) of the node table.
 * @param func a pointer to void function
 * @see cdd_postrehash_hook
 */
extern void cdd_prerehash_hook(void (*func)(void));

/**
 * Set post rehash hook. The hook is called after each rehashing of
 * the node table.
 * @param func a pointer to a function taking a CddRehashStat pointer argument
 * @see cdd_prerehash_hook
 * @see cdd_default_rehashhandler
 */
extern void cdd_postrehash_hook(void (*func)(CddRehashStat*));

/**
 * The default post GBC hook. It will print the GBC information to stderr.
 * @param info pointer to a GBC statistics structure.
 * @see cdd_postgbc_hook
 */
extern void cdd_default_gbhandler(CddGbcStat* info);

/**
 * The default post rehash hook. It will print the information to stderr.
 * @param info pointer to structure containing information about the rehash
 * @see cdd_postrehash_hook
 */
extern void cdd_default_rehashhandler(CddRehashStat* info);

/**
 * Trigger a garbage collector. The library will automatically run
 * the garbage collector when needed, but it can be triggered manually with
 * this function.
 */
extern void cdd_gbc();

/** @} */

// extern int32_t         cdd_setmaxnodenum(int);
// extern int32_t         cdd_setminfreenodes(int);

// extern int32_t         cdd_getnodenum();

/**
 * Creates a new CDD node corresponding to the constraint \a i - \a j
 * <~ \a bound, where \a i and \a j are indexes of clocks and \a bound
 * is an upper bound.
 * @param i a clock index
 * @param j a clock index
 * @param bound an upper bound
 * @return a CDD node encoding the constraint
 * @see cdd_interval
 */
extern ddNode* cdd_upper(int32_t i, int32_t j, raw_t bound);

/**
 * Creates a new CDD node corresponding to the constraint \a lower
 * <~ \a i - \a j <~ \a upper, where \a i and \a j are clock indexes and
 * \a lower and \a upper are bounds.
 * @param i a clock index
 * @param j a clock index
 * @param lower a lower bound
 * @param upper an upper bound
 * @return a CDD node encoding the interval
 * @see cdd_upper
 */
extern ddNode* cdd_interval(int32_t i, int32_t j, raw_t lower, raw_t upper);

/**
 * Creates a BDD node. The boolean variable is identified by the node
 * level. The level must correspond to a BDD variable (as opposed to
 * a clock difference).
 * @param level a node level
 * @return a BDD node
 */
extern ddNode* cdd_bddvar(int32_t level);

/**
 * Performs a binary operation on two decision diagrams.
 * @param left  the left argument to the operation
 * @param right the right argument to the operation
 * @param op    the binary operation to perform
 * @return the resulting decision diagram
 */
extern ddNode* cdd_apply(ddNode* left, ddNode* right, int32_t op);

/**
 * Performs a binary operation on two decision diagrams. The
 * result is in semi-canonical form.
 * @param left  the left argument to the operation
 * @param right the right argument to the operation
 * @param op    the binary operation to perform
 * @return the resulting decision diagram
 */
extern ddNode* cdd_apply_reduce(ddNode* left, ddNode* right, int32_t op);

/**
 * Brings a CDD into reduced form. The reduced form is pseudo
 * canonical in the sense that a tautology is represented by \c
 * cddtrue and an empty CDD by \c cddfalse. The reduced form is
 * guaranteed not to contain more paths than the original cdd, but it
 * might contain more nodes (due to reduced amount of sharing).
 *
 * @param cdd a cdd
 * @return a reduced cdd equivalent to \a cdd
 */
extern ddNode* cdd_reduce(ddNode* cdd);

/**
 * Returns the number of nodes in a decision diagram.
 * @param dd a decision diagram
 * @return the number of nodes in \a dd
 */
extern int32_t cdd_nodecount(ddNode* dd);

/**
 * Returns the number of edges in a decision diagram.
 * @param dd a decision diagram
 * @return the number of edges in \a dd.
 */
extern int32_t cdd_edgecount(ddNode* dd);

/**
 * Existential quantification. @todo
 */
extern ddNode* cdd_exist(ddNode*, int32_t*, int32_t*, int32_t, int32_t);

/**
 * Variable substitution. @todo
 */
extern ddNode* cdd_replace(ddNode*, int32_t*, int32_t*);

/**
 * If then else operation. @todo
 */
extern ddNode* cdd_ite(ddNode*, ddNode*, ddNode*);

/**
 * Another reduced form? @todo
 */
extern ddNode* cdd_reduce2(ddNode*);

/**
 * @name DBM related functions
 * These functions support conversion between CDDs and DBMs.
 * @{
 */

/**
 * Returns true if \a dbm is included in the CDD.
 * @param cdd a cdd
 * @param dbm a dbm
 * @return true if \a dbm is included in \a cdd
 */
extern int32_t cdd_contains(ddNode* cdd, raw_t* dbm, uint32_t dim);

/**
 * Convert a DBM to a CDD. It is important that the indexes of the DBM
 * correspond to clocks in the CDD library.
 * @param dbm a dbm
 * @return a CDD equivalent to \a dbm
 */
extern ddNode* cdd_from_dbm(const raw_t* dbm, uint32_t dim);

/**
 * Extract a zone from a CDD.  This function will extract a zone from
 * \a cdd and write it to \a dbm.  It will return a CDD equivalent to
 * \a cdd \ \c cdd_from_dbm(dbm).
 * PRECONDITION: call CDD reduce first!!!
 * @param cdd a cdd
 * @param dbm a dbm
 * @return the difference between \a cdd and \a dbm
 */
extern ddNode* cdd_extract_dbm(ddNode* cdd, raw_t* dbm, uint32_t dim);

/**
 * Extract a BDD from the bottom of a given CDD.
 * PRECONDITION: call CDD reduce first!!!
 * @param cdd a cdd
 * @param dbm a dbm
 * @return the difference between \a cdd and \a dbm
 */
extern ddNode* cdd_extract_bdd(ddNode* cdd, uint32_t dim);

/**
 * Print a CDD \a r as a dot input file \a ofile.\n\n
 *
 * Terminal nodes are printed as square nodes, where \a 1 represents the
 * true node and \a 0 the false node.\n
 *
 * Clock-difference nodes are printed as octagons. All edges going to the
 * false terminal node are omitted from being printed, as there might
 * be a lot of them per clock difference.\n
 *
 * Boolean nodes are printed as circles.\n\n
 *
 * Negated nodes are printed in red. When \a push_negate is set to true, the
 * negation of nodes is taken into account while printing the cdd. So the
 * reached terminal node is the correct value. When \a push_negate is set
 * to false, the cdd is printed as-is. The correct value for a particular
 * variable assignment can be determined by counting the number of negated
 * nodes (printed in red) along the path from the root node to the terminal
 * and then negating the found terminal node if the counted number of negated
 * nodes is odd.\n\n
 *
 * You can use, for example, the dot program from the graphwiz package to
 * convert this to a PS picture of the CDD.
 *
 * @param ofile the file to write to.
 * @param cdd   a CDD.
 * @param push_negate when true printing takes negation into account, when
 *      false the cdd is printed as-is.
 */
extern void cdd_fprintdot(FILE* ofile, ddNode* cdd, bool push_negate);

/**
 * Print a CDD \a r as a dot input file to stdout.
 * @see cdd_fprintdot
 * @param cdd   a CDD.
 * @param push_negate when true printing takes negation into account, when
 *      false the cdd is printed as-is.
 */
extern void cdd_printdot(ddNode* cdd, bool push_negate);

/**
 * Dump all the CDD nodes.
 */
extern void cdd_dump_nodes();

typedef void (*cdd_print_varloc_f)(FILE* out, uint32_t* mask, uint32_t* value, void* data, int32_t size);

typedef void (*cdd_print_clockdiff_f)(FILE* out, int32_t x1, int32_t x2, void* data);

/**
 * Another way of printing a BCDD - reduced form
 * @param cdd   a CDD, a BDD or a BCDD with upper part being a
 * BDD and lower part a CDD
 */
extern void cdd_fprint_code(FILE* ofile, ddNode* cdd, cdd_print_varloc_f printer1, cdd_print_clockdiff_f printer2,
                            void* data);
extern void cdd_fprint_graph(FILE* ofile, ddNode* cdd, cdd_print_varloc_f printer1, cdd_print_clockdiff_f printer2,
                             void* data);

/** @} */

/**
 * The empty decision diagram.
 */
extern ddNode* cddfalse;

/**
 * The negation of \c cddfalse.
 */
extern ddNode* cddtrue;

#ifdef __cplusplus
}
#endif

/**
 * @name Error codes
 * @{
 */
#define CDD_MEMORY  (-1)  /**< Out of memory */
#define CDD_VAR     (-2)  /**< Unknown variable */
#define CDD_RANGE   (-3)  /**< Variable value out of range (not in domain) */
#define CDD_DEREF   (-4)  /**< Removing external reference to unknown node */
#define CDD_RUNNING (-5)  /**< Called cdd_init() twice whithout cdd_done() */
#define CDD_FILE    (-6)  /**< Some file operation failed */
#define CDD_FORMAT  (-7)  /**< Incorrect file format */
#define CDD_ORDER   (-8)  /**< Variables not in order for vector based functions */
#define CDD_BREAK   (-9)  /**< User called break */
#define CDD_CLKNUM  (-10) /**< Different number of variables for vector pair */
#define CDD_NODES                                                               \
    (-11)                       /**< Tried to set maximum number of nodes to be \
                                     fewer than there already has been allocated */
#define CDD_OP            (-12) /**< Unknown operator */
#define CDD_CLKSET        (-13) /**< Illegal variable set */
#define CDD_OVERLAP       (-14) /**< Overlapping variable blocks */
#define CDD_DECCNUM       (-15) /**< Trying to decrease the number of variables */
#define CDD_REPLACE       (-16) /**< Replacing to already existing variables */
#define CDD_NODENUM       (-17) /**< Number of nodes reached user defined maximum */
#define CDD_ILLCDD        (-18) /**< Illegal cdd argument */
#define CDD_STACKOVERFLOW (-19) /**< Reference stack overflow */
#define CDD_NODE          (-20) /**< Invalid node type */
#define CDD_MAXSIZE       (-21) /**< CDD Node larger than maximum allowed */

#define CDD_ERRNUM 21

/** @} error codes */

/** @} C interface */

#ifdef __cplusplus

/**
 * @defgroup cplusplus The C++ interface
 *
 * The C++ interface provides a cdd class representing a decision
 * diagram (either BDD, CDD or both). The class overrides a number of
 * operators providing primitive operations such as conjunction and
 * disjunction.
 *
 * The interface also provides a number of friend functions, which
 * supersede many of the function in the C interface, but the C++
 * interface does not replace the C interface, e.g. the initialisation
 * functions of the C interface are not duplicated in the C++ interface.
 *
 * One important difference to the C interface is that the cdd class
 * updates the reference counter of the decision diagram. For this
 * reason you should use the C++ interface whenever possible.
 *
 * Notice that the C++ interface uses some preprocessor macros to
 * avoid that you end up calling similar functions in the C interface,
 * e.g. calls to \c cdd_upper() are rewritten to call \c cdd_upperpp()
 * instead.
 *
 * @{
 */

struct extraction_result;
struct bdd_arrays;
/**
 * C++ encapsulation of a decision diagram node (a ddNode). The class
 * maintains a reference to the node throughout its lifetime.
 */
class cdd
{
public:
    /**
     * Default constructor. Constructs a NULL decision tree.
     */
    cdd() { assert(cdd_isrunning()); }

    /**
     * Copy constructor.
     * @param r another cdd
     */
    cdd(const cdd& r);

    /**
     * Construct from DBM.
     */
    cdd(const raw_t* dbm, uint32_t dim);

    /**
     * Construct cdd object by wrapping a ddNode pointer.
     * @param r a ddNode
     */
    explicit cdd(ddNode* r);

    /**
     * Destructor. The destructor decrements the reference count
     * on the decision diagram.
     */
    ~cdd();

    /**
     * Returns the inner ddNode pointer. The reference to the node
     * is not automatically incremented.
     * @return a ddNode pointer
     */
    [[nodiscard]] ddNode* handle() const { return root; }

    /**
     * Assignment operator.
     * @param r a cdd
     * @return this
     */
    cdd& operator=(const cdd& r);

    /**
     * Assignment operator for conjunction.
     */
    cdd operator&=(const cdd& r);

    /**
     * Disjunction operator.
     */
    cdd operator|(const cdd& r) const;

    /**
     * Assignment operator for disjunction.
     */
    cdd operator|=(const cdd& r);

    /**
     * Set minus operator. The result is the set minus between the
     * left and the right operand.
     */
    cdd operator-(const cdd& r) const;

    /**
     * Assignment operator for set minus.
     */
    cdd operator-=(const cdd& r);

    /**
     * XOR operator.
     */
    cdd operator^(const cdd& r) const;

    /**
     * Assignment operator for XOR.
     */
    cdd operator^=(const cdd& r);

    /**
     * Unary negation. This computes the complement of the decision diagram.
     */
    cdd operator!() const;

    /**
     * Equality operator. This is a constant time test and only
     * semantically well-defined for ROBDDs. CDDs do not have an
     * efficient normal form and therefore you should test equality
     * with something like <tt>cdd_reduce(l ^ r) ==
     * cdd_false()</tt>. Notice how this test uses the equality
     * operator to compare a CDD in the pseudo normal form with the
     * empty CDD.
     */
    int32_t operator==(const cdd& r) const;

    /**
     * Inequality operator. The same restrictions as for the equality
     * operator apply.
     * @see operator==
     */
    int32_t operator!=(const cdd& r) const;

private:
    ddNode* root{cddfalse};

    cdd operator=(ddNode* r);

    friend cdd cdd_true();
    friend cdd cdd_false();
    friend cdd cdd_upperpp(int, int, raw_t);
    friend cdd cdd_lowerpp(int, int, raw_t);
    friend cdd cdd_intervalpp(int, int, raw_t, raw_t);
    friend cdd cdd_bddvarpp(int);
    friend cdd cdd_bddnvarpp(int);
    friend cdd cdd_remove_negative(const cdd& node);
    friend cdd cdd_exist(const cdd&, int32_t*, int32_t*, int32_t, int32_t);
    friend cdd cdd_replace(const cdd&, int32_t*, int32_t*);
    friend int32_t cdd_nodecount(const cdd&);
    friend cdd cdd_apply(const cdd&, const cdd&, int);
    friend cdd cdd_apply_reduce(const cdd&, const cdd&, int);
    friend cdd cdd_ite(const cdd&, const cdd&, const cdd&);
    friend cdd cdd_reduce(const cdd&);
    friend bool cdd_equiv(const cdd&, const cdd&);
    friend cdd cdd_delay(const cdd&);
    friend cdd cdd_past(const cdd&);
    friend bool cdd_isBDD(const cdd&);
    friend bdd_arrays cdd_bdd_to_array(const cdd&);
    friend cdd cdd_delay_invariant(const cdd&, const cdd&);
    friend cdd cdd_apply_reset(const cdd& state, int32_t* clock_resets, int32_t* clock_values, int32_t num_clock_resets,
                               int32_t* bool_resets, int32_t* bool_values, int32_t num_bool_resets);
    friend cdd cdd_transition(const cdd& state, const cdd& guard, int32_t* clock_resets, int32_t* clock_values,
                              int32_t num_clock_resets, int32_t* bool_resets, int32_t* bool_values,
                              int32_t num_bool_resets);
    friend cdd cdd_transition_back(const cdd& state, const cdd& guard, const cdd& update, int32_t* clock_resets,
                                   int32_t num_clock_resets, int32_t* bool_resets, int32_t num_bool_resets);
    friend cdd cdd_transition_back_past(const cdd& state, const cdd& guard, const cdd& update, int32_t* clock_resets,
                                        int32_t num_clock_resets, int32_t* bool_resets, int32_t num_bool_resets);
    friend cdd cdd_predt(const cdd& target, const cdd& safe);
    friend cdd cdd_reduce2(const cdd&);
    friend bool cdd_contains(const cdd&, raw_t* dbm, uint32_t dim);
    friend cdd cdd_extract_dbm(const cdd&, raw_t* dbm, uint32_t dim);
    friend cdd cdd_extract_bdd(const cdd&, uint32_t dim);
    friend extraction_result cdd_extract_bdd_and_dbm(const cdd&);
    friend void cdd_fprintdot(FILE* ofile, const cdd&, bool push_negate);
    friend void cdd_printdot(const cdd&, bool push_negate);
    friend void cdd_fprint_code(FILE* ofile, const cdd&, cdd_print_varloc_f printer1, cdd_print_clockdiff_f printer2,
                                void* dict);
    friend void cdd_fprint_graph(FILE* ofile, const cdd&, cdd_print_varloc_f printer1, cdd_print_clockdiff_f printer2,
                                 void* dict);
#ifdef MULTI_TERMINAL
    friend cdd cdd_apply_tautology(const cdd&, int32_t);
    friend int32_t cdd_get_tautology_id(const cdd&);
    friend int32_t cdd_eval_true(const cdd&);
    friend int32_t cdd_eval_false(const cdd&);
#endif
};

/*=== Inline C++ interface ============================================*/

/** Structure for returning the results of extractDBM */
typedef struct extraction_result
{
    cdd CDD_part; /**< The remainder of the CDD after removing a DBM */
    cdd BDD_part; /**< The boolean part below a removed DBM */
    raw_t* dbm;   /**< The removed DBM */
} extraction_result;

/** Structure for returning the logical formula of a BDD. */
typedef struct bdd_arrays
{
    int32_t* vars;     /** The variables that are relevant. Size = numTraces x numBools. */
    int32_t* values;   /** The value the variables have. Size = numTraces x numBools. */
    int32_t numTraces; /** The number of traces collected. */
    int32_t numBools;  /** The number of maximum boolean variables per trace.*/
} bdd_arrays;

/**
 * Returns true if \a dbm is included in the CDD.
 * @param c a cdd
 * @param d a dbm
 * @return true if \a dbm is included in \a cdd
 */
inline bool cdd_contains(const cdd& c, raw_t* dbm, uint32_t dim) { return cdd_contains(c.handle(), dbm, dim); }

/**
 * AND operator. Computes the conjunction of the two operands.
 */
inline cdd operator&(const cdd& l, const cdd& r) { return cdd_apply(l, r, cddop_and); }

/**
 * If-then-else operator.
 */
inline cdd cdd_ite(const cdd& f, const cdd& g, const cdd& h) { return (f & g) | ((!f) & h); }

/**
 * Creates a new CDD node corresponding to the constraint \a i - \a j
 * <~ \a bound, where \a i and \a j are indexes of clocks and \a bound
 * is a upper bound.
 * @param i a clock index
 * @param j a clock index
 * @param bound an upper bound
 * @return a CDD node encoding the constraint
 * @see cdd_intervalpp
 */
inline cdd cdd_upperpp(int32_t i, int32_t j, raw_t bound) { return cdd(cdd_upper(i, j, bound)); }

/**
 * Creates a new CDD node corresponding to the constraint bound <~ \a
 * i - \a j, where \a i and \a j are indexes of clocks and \a bound is
 * a lower bound.
 * @param i a clock index
 * @param j a clock index
 * @param bound a lower bound
 * @return a CDD node encoding the constraint
 * @see cdd_intervalpp
 */
inline cdd cdd_lowerpp(int32_t i, int32_t j, raw_t bound) { return cdd(cdd_neg(cdd_upper(i, j, bound))); }

/**
 * Interprate the CDD as clock values, and remove any negative
 * clock values.
 * @param the original cdd
 * @return a cdd that does not contain negative value
 */
inline cdd cdd_remove_negative(const cdd& node) { return cdd(cdd_remove_negative(node.handle())); }

/**
 * Checks for equivalence between two CDDs.
 */
inline bool cdd_equiv(const cdd& l, const cdd& r) { return cdd_equiv(l.root, r.root); }

/**
 * Creates a new CDD node corresponding to the constraint \a lower
 * <~ \a i - \a j <~ \a upper, where \a i and \a j are clock indexes and
 * \a lower and \a upper are bounds.
 * @param i a clock index
 * @param j a clock index
 * @param lower a lower bound
 * @param upper an upper bound
 * @return a CDD node encoding the interval
 * @see cdd_upper
 */
inline cdd cdd_intervalpp(int32_t i, int32_t j, raw_t low, raw_t up) { return cdd(cdd_interval(i, j, low, up)); }

/**
 * Creates a BDD node. The boolean variable is identified by the node
 * level. The level must correspond to a BDD variable (as opposed to
 * a clock difference).
 * @param level node level
 * @return a BDD node
 */
inline cdd cdd_bddvarpp(int32_t level) { return cdd(cdd_bddvar(level)); }

/**
 * Like \c bddvarpp(), but the result is negated
 * @param level node level
 * @return a BDD node
 */
inline cdd cdd_bddnvarpp(int32_t level) { return cdd(cdd_neg(cdd_bddvar(level))); }

/**
 * Returns the CDD corresponding to a tautology.
 * @return the cdd corresponding to a tautology.
 */
inline cdd cdd_true() { return cdd(cddtrue); }

/**
 * Returns the empty CDD.
 * @return the empty CDD.
 */
inline cdd cdd_false() { return cdd(cddfalse); }

/**
 * Existential quantification.
 * @param r the cdd
 * @param levels array of boolean node levels that have to be existentially quantified
 * @param clocks array of clock numbers that have to be existentially quantified
 * @param num_bools the number of boolean variables that have to be existentially quantified,
 *      should match the size of \a levels
 * @param num_clocks the number of clock variables that have to be existentially quantified,
 *      should match the size of \a clocks
 * @return a cdd where the boolean variables from \a levels and the clock variables from \a clocks
 *      are existentially quantified.
 */
inline cdd cdd_exist(const cdd& r, int32_t* levels, int32_t* clocks, int32_t num_bools, int32_t num_clocks)
{
    return cdd(cdd_exist(r.root, levels, clocks, num_bools, num_clocks));
}

/**
 * Variable substitution.
 * @todo
 */
inline cdd cdd_replace(const cdd& r, int32_t* f, int32_t* g) { return cdd(cdd_replace(r.root, f, g)); }

/**
 * Returns the number of nodes (size) of the CDD.
 * @param r a CDD
 * @return the size of \a r
 */
inline int32_t cdd_nodecount(const cdd& r) { return cdd_nodecount(r.root); }

/**
 * Performs a binary operation on two decision diagrams.
 * @param left  the left argument to the operation
 * @param right the right argument to the operation
 * @param op    the binary operation to perform
 * @return the resulting decision diagram
 */
inline cdd cdd_apply(const cdd& left, const cdd& right, int32_t op)
{
    return cdd(cdd_apply(left.root, right.root, op));
}

/**
 * Performs a binary operation on two decision diagrams. The
 * result is in semi-canonical form.
 * @param left  the left argument to the operation
 * @param right the right argument to the operation
 * @param op    the binary operation to perform
 * @return the resulting decision diagram
 */
inline cdd cdd_apply_reduce(const cdd& left, const cdd& right, int32_t op)
{
    return cdd(cdd_apply_reduce(left.root, right.root, op));
}

/**
 * Brings a CDD into reduced form. The reduced form is pseudo
 * canonical in the sense that a tautology is represented by \c
 * cddtrue and an empty CDD by \c cddfalse. The reduced form is
 * guaranteed to not be bigger than the original cdd.
 * @param r a cdd
 * @return a reduced cdd equivalent to \a cdd
 */
inline cdd cdd_reduce(const cdd& r) { return cdd(cdd_reduce(r.root)); }

/**
 * Computes reduced form.
 * @todo
 */
inline cdd cdd_reduce2(const cdd& r) { return cdd(cdd_reduce2(r.root)); }

/**
 * Extract a zone from a CDD.  This function will extract a zone from
 * \a cdd and write it to \a dbm.  It will return a CDD equivalent to
 * \a cdd \ \c cdd_from_dbm(dbm).
 * @param cdd a cdd
 * @param dbm a dbm
 * @param dim the dimension of the dbm
 * @return the difference between \a cdd and \a dbm
 */
inline cdd cdd_extract_dbm(const cdd& r, raw_t* dbm, uint32_t dim)
{
    return cdd(cdd_extract_dbm(r.handle(), dbm, dim));
}

/**
 * Extract the bottom BDD of the first DBM in a given CDD.
 * @param cdd a cdd
 * @param dim the dimension of the dbm
 * @return the difference between \a cdd and \a dbm
 */
inline cdd cdd_extract_bdd(const cdd& r, uint32_t dim) { return cdd(cdd_extract_bdd(r.handle(), dim)); }

/**
 * Print a CDD \a r as a dot input file \a ofile. You can use the dot
 * program from the graphwiz package to convert this to a PS picture
 * of the CDD.
 * @param ofile the file to write to.
 * @param cdd   a CDD.
 */
inline void cdd_fprintdot(FILE* ofile, const cdd& cdd, bool push_negate)
{
    cdd_fprintdot(ofile, cdd.root, push_negate);
}

/**
 * Print a CDD \a r as a dot input file to stdout.
 * @see cdd_fprintdot
 * @param cdd   a CDD.
 */
inline void cdd_printdot(const cdd& cdd, bool push_negate) { cdd_printdot(cdd.root, push_negate); }

/**
 * Another way of printing a BCDD
 * @param cdd   a CDD, a BDD or a BCDD with upper part being a
 * BDD and lower part a CDD
 */
inline void cdd_fprint_code(FILE* ofile, const cdd& cdd, cdd_print_varloc_f printer1, cdd_print_clockdiff_f printer2,
                            void* data)
{
    cdd_fprint_code(ofile, cdd.root, printer1, printer2, data);
}
inline void cdd_fprint_graph(FILE* ofile, const cdd& cdd, cdd_print_varloc_f printer1, cdd_print_clockdiff_f printer2,
                             void* data)
{
    cdd_fprint_graph(ofile, cdd.root, printer1, printer2, data);
}

#ifdef MULTI_TERMINAL
inline cdd cdd_apply_tautology(const cdd& dd, int32_t t_id) { return cdd(cdd_apply_tautology(dd.root, t_id)); }

inline int32_t cdd_get_tautology_id(const cdd& cdd) { return cdd_get_tautology_id(cdd.root); }

inline int32_t cdd_eval_true(const cdd& cdd) { return cdd_eval_true(cdd.root); }

inline int32_t cdd_eval_false(const cdd& cdd) { return cdd_eval_false(cdd.root); }
#endif

/**
 * @name Defines
 * Macros to rewrite calls to some of the C functions to call
 * the C++ version instead.
 * @{
 */
#define cdd_lower    cdd_lowerpp
#define cdd_upper    cdd_upperpp
#define cdd_interval cdd_intervalpp
#define cdd_bddvar   cdd_bddvarpp
#define cdd_bddnvar  cdd_bddnvarpp
//#define cdd_nodecount cdd_nodecountpp

/** @} */

/*
inline cdd cdd::operator&(const cdd &r) const
{ return cdd_apply(*this, r, cddop_and); }
*/

inline cdd cdd::operator&=(const cdd& r) { return (*this = *this & r); }

inline cdd cdd::operator|(const cdd& r) const { return !cdd_apply(!*this, !r, cddop_and); }

inline cdd cdd::operator|=(const cdd& r) { return (*this = *this | r); }

inline cdd cdd::operator-(const cdd& r) const { return cdd_apply(*this, !r, cddop_and); }

inline cdd cdd::operator-=(const cdd& r) { return (*this = *this - r); }

inline cdd cdd::operator^(const cdd& r) const { return cdd_apply(*this, r, cddop_xor); }

inline cdd cdd::operator^=(const cdd& r) { return (*this = *this ^ r); }

inline cdd cdd::operator!() const { return cdd(cdd_neg(root)); }

inline int32_t cdd::operator==(const cdd& r) const { return r.root == root; }

inline int32_t cdd::operator!=(const cdd& r) const { return r.root != root; }

/** @} cplusplus */

#endif /* CPLUSPLUS */

#endif
