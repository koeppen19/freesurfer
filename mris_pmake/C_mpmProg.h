/**
 * @file  C_mpmProg.h
 * @brief The internal 'program' API.
 *
 *  C_mpmProgs are overloaded classes that perform specific functions in the
 *  context of the Dijkstra system. The default contol system allows an
 *  external network-based program to drive the core dijkstra search. This
 *  process can be very slow on large-scale problems. To alleviate that, 
 *  mpm_programs can be written. These map directly to external dsh scripts
 *  but without any of the network overhead.
 */
/*
 * Original Author: Rudolph Pienaar
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2012/11/14 22:09:04 $
 *    $Revision: 1.10.2.1 $
 *
 * Copyright © 2011 The General Hospital Corporation (Boston, MA) "MGH"
 *
 * Terms and conditions for use, reproduction, distribution and contribution
 * are found in the 'FreeSurfer Software License Agreement' contained
 * in the file 'LICENSE' found in the FreeSurfer distribution, and here:
 *
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferSoftwareLicense
 *
 * Reporting: freesurfer@nmr.mgh.harvard.edu
 *
 */


#ifndef __C_MPM_PROG_H__
#define __C_MPM_PROG_H__

#ifdef __cplusplus
extern  "C" {
#endif

#include "mri.h"
#include "mrisurf.h"
#include "label.h"
#include "error.h"
#include "fio.h"

#ifdef __cplusplus
}
#endif

#include "env.h"

#include "oclDijkstraKernel.h"

#include <string>
using namespace std;

const int       MPMSTACKDEPTH     = 64;

class C_mpmProg {

    //
    // Data members
    //

  public:

    // type and var info
    string      mstr_obj;                       // name of object class
    string      mstr_name;                      // name of object variable
    int         mid;                            // id of socket
    int         mverbosity;                     // Debug related value 
    int         mwarnings;                      // Show warnings
    int         mstackDepth;                    // Current procedure stackDepth
    string      mstr_proc[MPMSTACKDEPTH];       // Used to track the current
                                                //+ procedure being executed
  protected:

    // base class info

    s_env*      mps_env;                        // Pointer to the main
                                                //+ environment
    bool        b_created;                                                                   

    //
    // Method members
    //

  public:

    //
    // Constructor / destructor block
    //
    void core_construct(
        string          astr_name       = "unnamed",
        int             a_id            = -1,
        int             a_verbosity     = 0,
        int             a_warnings      = 0,
        int             a_stackDepth    = 0,
        string          astr_proc       = "noproc"
    );
    C_mpmProg(s_env* aps_env);
    virtual ~C_mpmProg(void);
    C_mpmProg(
        const C_mpmProg & C_mpmProg
    );
    C_mpmProg & operator=(const C_mpmProg & C_mpmProg);

    //
    // Error / warn /  print block
    //
    void        debug_push(     string  astr_currentProc);
    void        debug_pop();
    void        error(          string  astr_msg        = "",
                                int     code            = 1);
    void        warn(           string  astr_msg        = "",
                                int     code            = 1);
    void        function_trace( string  astr_msg        = "",
                                string  astr_separator  = "");
    void        print();

    //
    // Access "housekeeping" state info
    //

    const string        str_obj_get()           const {
        return mstr_obj;
    };
    const string        str_name_get()          const {
        return mstr_name;
    };
    const int           id_get()                const {
        return mid;
    };
    const int           verbosity_get()         const {
        return mverbosity;
    };
    const int           warnings_get()          const {
        return mwarnings;
    };
    const int           stackDepth_get()        const {
        return mstackDepth;
    };

    void                str_obj_set(string astr_val) {
        mstr_obj        = astr_val;
    };
    void                str_name_set(string astr_val) {
        mstr_name       = astr_val;
    };
    void                str_proc_set(int depth, string astr_proc) {
        mstr_proc[depth] = astr_proc;
    } ;
    void                id_set(int value) {
        mid             = value ;
    } ;
    void                verbosity_set(int value) {
        mverbosity      = value ;
    } ;
    void                warnings_set(int value) {
        mwarnings       = value ;
    } ;
    void                stackDepth_set(int value) {
        mstackDepth     = value ;
    } ;
    const string        str_proc_get()          const {
        return mstr_proc[stackDepth_get()];
    };
    const string        str_proc_get(int i) {
        return mstr_proc[i];
    };

    //
    // Core class methods
    //

    virtual int         run(void)                = 0;

};

class C_mpmProg_NOP : public C_mpmProg {

  protected:

    int         msleepSeconds;

 public:
    C_mpmProg_NOP(s_env* aps_env);
    ~C_mpmProg_NOP(void);

    //
    // Access block
    //
    void        sleepSeconds_set(int avalue) {msleepSeconds = avalue;};
    int         sleepSeconds_get()      const {return(msleepSeconds);};

    //
    // Functional block
    //

    virtual int         run(void);
};

///
/// \class C_mpmProg_pathFind
/// \brief This class simply finds a path between two vertices.
///
class C_mpmProg_pathFind : public C_mpmProg {

  protected:

    int         mvertex_start;
    int         mvertex_end;
    int         mvertex_total;
    bool        mb_surfaceRipClear;
      
  public:
    C_mpmProg_pathFind(
        s_env* 		aps_env, 
        int 		amvertex_start = 0, 
        int 		amvertex_end = -1);
    ~C_mpmProg_pathFind(void);

    //
    // Access block
    //
    void        surfaceRipClear_set(bool avalue) {
            mb_surfaceRipClear  = avalue;
    };
    int         surfaceRipClear_get() {
            return(mb_surfaceRipClear);
    };
    void        vertexStart_set(int avalue) {
            mvertex_start       	= avalue;
	    mps_env->startVertex	= avalue;
    };
    int         vertexStart_get() {
            return(mvertex_start);
    };
    void        vertexEnd_set(int avalue) {
            mvertex_end         	= avalue;
	    mps_env->endVertex		= avalue;
    };
    int         vertexEnd_get() {
            return(mvertex_end);
    };
    void        print(void);

    //
    // Functional block
    //

    virtual int         run(void);
    float               cost_compute(int start, int end);
};

///
/// \class C_mpmProg_autodijk
/// \brief This class implements the Dijkstra algorithm on the CPU.
///
class C_mpmProg_autodijk : public C_mpmProg {

  protected:

    int         mvertex_polar;
    int         mvertex_start;
    int         mvertex_step;
    int         mvertex_end;
    int         mvertex_total;
    int         m_costFunctionIndex;
    bool        mb_surfaceRipClear;
    bool        mb_performExhaustive;           // If true, perform cost
                                                //+ calculations from polar
                                                //+ to every other vertex in
                                                //+ mesh -- useful only for
                                                //+ debugging/memory testing
                                                //+ since a search from 
                                                //+ vertex->vertex will cover
                                                //+ whole mesh anyway in single
                                                //+ sweep.
    int         mprogressIter;                  // Number of iterations to
                                                //+ loop before showing
                                                //+ progress to stdout
    float*      mpf_cost;                       // Cost as calculated by
                                                //+ autodijk
    string      mstr_costFileName;              // Parsed from the environment
                                                //+ structure
    string      mstr_costFullPath;              // Full path to cost file                                            
      
  public:
    C_mpmProg_autodijk(s_env* aps_env);
    ~C_mpmProg_autodijk(void);

    //
    // Access block
    //
    void        surfaceRipClear_set(bool avalue) {
            mb_surfaceRipClear  = avalue;
    };
    int         surfaceRipClear_get() {
            return(mb_surfaceRipClear);
    };

    void        costFile_set(string avalue) {
        mstr_costFileName   = avalue;
        mstr_costFullPath   = mps_env->str_workingDir + "/" + mstr_costFileName;
    };
    string      costFile_get() {
            return(mstr_costFileName);
    };
    
    void        vertexPolar_set(int avalue) {
            mvertex_polar       = avalue;
    };
    int         vertexPolar_get() {
            return(mvertex_polar);
    };
    void        vertexStart_set(int avalue) {
            mvertex_start       = avalue;
    };
    int         vertexStart_get() {
            return(mvertex_start);
    };
    void        vertexStep_set(int avalue) {
            mvertex_step        = avalue;
    };
    int         vertexStep_get() {
            return(mvertex_step);
    };
    void        vertexEnd_set(int avalue) {
            mvertex_end         = avalue;
    };
    int         vertexEnd_get() {
            return(mvertex_end);
    };
    void        progressIter_set(int avalue) {
            mprogressIter       = avalue;
    };
    int         progressIter_get() {
            return(mprogressIter);
    };
    void        print(void);

    //
    // Functional block
    //

    virtual int         run(void);
    float               cost_compute(int start, int end);
    e_FILEACCESS        CURV_fileWrite();
};

///
/// \class C_mpmProg_autodijk_fast
/// \brief This class implements the Dijkstra algorithm using a fast implementation
///        of the Single Source Shortest Path algorithm.  If compiled with OpenCL, it
///        will run a parallel accelerated version of the algorithm.  If not, it will
///        just run a single threaded fast version of the algorithm on the CPU
///
class C_mpmProg_autodijk_fast : public C_mpmProg_autodijk 
{
  protected:

    ///
    /// Convert the freesurfer environment MRI surface representation into
    /// a representation that is formatted for consumption by the OpenCL
    /// algorithm
    /// \param graph Pointer to the graph data to generate
    ///
    void genOpenCLGraphRepresentation(GraphData *graph);

  public:
    ///
    //  Constructor
    //
    C_mpmProg_autodijk_fast(s_env* aps_env);
    
    ///
    //  Destructor
    //
    virtual ~C_mpmProg_autodijk_fast(void);

    //
    // Functional block
    //
    virtual int         run(void);
};


#endif //__C_MPM_PROG_H__


