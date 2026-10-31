/*
** 2010 August 28
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** Code for testing all sorts of SQLite interfaces. This code
** is not included in the SQLite library. 
*/

#include "sqlite3.h"
#include "tclsqlite.h"

/* Solely for the UNUSED_PARAMETER() macro. */
#include "sqliteInt.h"

#ifdef SQLITE_ENABLE_RTREE

extern const char *sqlite3ErrStr(int);

typedef struct BoxGeomCtx BoxGeomCtx;
struct BoxGeomCtx {
  Tcl_Interp *interp;
  Tcl_Obj *pScript;
};

typedef struct BoxQueryCtx BoxQueryCtx;
struct BoxQueryCtx {
  Tcl_Interp *interp;
  Tcl_Obj *pScript;
};

static int rtreeGetDbPointer(
  Tcl_Interp *interp,
  const char *zName,
  sqlite3 **pDb
){
  Tcl_CmdInfo info;
  if( 0==Tcl_GetCommandInfo(interp, zName, &info) ){
    Tcl_AppendResult(interp, "no such handle: ", zName, NULL);
    return TCL_ERROR;
  }
  *pDb = *(sqlite3 **)info.objClientData;
  return TCL_OK;
}

static void testDelUser(void *pCtx){
  BoxGeomCtx *p = (BoxGeomCtx*)pCtx;
  Tcl_EvalObjEx(p->interp, p->pScript, 0);
  Tcl_DecrRefCount(p->pScript);
  sqlite3_free(p);
}

static int invokeTclGeomCb(
  const char *zName, 
  sqlite3_rtree_geometry *p, 
  int nCoord,
  sqlite3_rtree_dbl *aCoord
){
  int rc = SQLITE_OK;
  if( p->pContext ){
    char aPtr[64];
    BoxGeomCtx *pCtx = (BoxGeomCtx*)p->pContext;
    Tcl_Interp *interp = pCtx->interp;
    Tcl_Obj *pScript = 0;
    Tcl_Obj *pParam = 0;
    Tcl_Obj *pCoord = 0;
    int ii;
    Tcl_Obj *pRes;


    pScript = Tcl_DuplicateObj(pCtx->pScript);
    Tcl_IncrRefCount(pScript);
    Tcl_ListObjAppendElement(interp, pScript, Tcl_NewStringObj(zName,-1));

    sqlite3_snprintf(sizeof(aPtr)-1, aPtr, "%p", (void*)p->pContext);
    Tcl_ListObjAppendElement(interp, pScript, Tcl_NewStringObj(aPtr,-1));

    pParam = Tcl_NewObj();
    for(ii=0; ii<p->nParam; ii++){
      Tcl_ListObjAppendElement(
          interp, pParam, Tcl_NewDoubleObj(p->aParam[ii])
      );
    }
    Tcl_ListObjAppendElement(interp, pScript, pParam);

    pCoord = Tcl_NewObj();
    for(ii=0; ii<nCoord; ii++){
      Tcl_ListObjAppendElement(interp, pCoord, Tcl_NewDoubleObj(aCoord[ii]));
    }
    Tcl_ListObjAppendElement(interp, pScript, pCoord);

    sqlite3_snprintf(sizeof(aPtr)-1, aPtr, "%p", (void*)p);
    Tcl_ListObjAppendElement(interp, pScript, Tcl_NewStringObj(aPtr,-1));

    rc = Tcl_EvalObjEx(interp, pScript, 0);
    if( rc!=TCL_OK ){
      rc = SQLITE_ERROR;
    }else{
      Tcl_Size nObj = 0;
      Tcl_Obj **aObj = 0;

      pRes = Tcl_GetObjResult(interp);
      if( Tcl_ListObjGetElements(interp, pRes, &nObj, &aObj) ) return TCL_ERROR;
      if( nObj>0 ){
        const char *zCmd = Tcl_GetString(aObj[0]);
        if( 0==sqlite3_stricmp(zCmd, "zero") ){
          p->aParam[0] = 0.0;
          p->nParam = 1;
        }
        else if( 0==sqlite3_stricmp(zCmd, "user") ){
          if( p->pUser || p->xDelUser ){
            rc = SQLITE_ERROR;
          }else{
            BoxGeomCtx *pBGCtx = sqlite3_malloc(sizeof(BoxGeomCtx));
            if( pBGCtx==0 ){
              rc = SQLITE_NOMEM;
            }else{
              pBGCtx->interp = interp;
              pBGCtx->pScript = Tcl_DuplicateObj(pRes);
              Tcl_IncrRefCount(pBGCtx->pScript);
              Tcl_ListObjReplace(interp, pBGCtx->pScript, 0, 1, 0, 0);
              p->pUser = (void*)pBGCtx;
              p->xDelUser = testDelUser;
            }
          }
        }
        else if( 0==sqlite3_stricmp(zCmd, "user_is_zero") ){
          if( p->pUser || p->xDelUser ) rc = SQLITE_ERROR;
        }
      }
    }
  }
  return rc;
}

/*
# EVIDENCE-OF: R-00693-36727 The legacy xGeom callback is invoked with
# four arguments.

# EVIDENCE-OF: R-50437-53270 The first argument is a pointer to an
# sqlite3_rtree_geometry structure which provides information about how
# the SQL function was invoked.

# EVIDENCE-OF: R-00090-24248 The third argument, aCoord[], is an array
# of nCoord coordinates that defines a bounding box to be tested.

# EVIDENCE-OF: R-28207-40885 The last argument is a pointer into which
# the callback result should be written.

*/
static int box_geom(
  sqlite3_rtree_geometry *p,      /* R-50437-53270 */
  int nCoord,                     /* R-02424-24769 */
  sqlite3_rtree_dbl *aCoord,      /* R-00090-24248 */
  int *pRes                       /* R-28207-40885 */
){
  int ii;

  if( p->nParam!=nCoord ){
    invokeTclGeomCb("box", p, nCoord, aCoord);
    return SQLITE_ERROR;
  }
  if( invokeTclGeomCb("box", p, nCoord, aCoord) ) return SQLITE_ERROR;

  for(ii=0; ii<nCoord; ii+=2){
    if( aCoord[ii]>p->aParam[ii+1] || aCoord[ii+1]<p->aParam[ii] ){
      /* R-28207-40885 */
      *pRes = 0;
      return SQLITE_OK;
    }
  }

  /* R-28207-40885 */
  *pRes = 1;

  return SQLITE_OK;
}

static int SQLITE_TCLAPI register_box_geom(
  void * clientData,
  Tcl_Interp *interp,
  int objc,
  Tcl_Obj *CONST objv[]
){
  sqlite3 *db;
  BoxGeomCtx *pCtx;
  char aPtr[64];
  UNUSED_PARAMETER(clientData);

  if( objc!=3 ){
    Tcl_WrongNumArgs(interp, 1, objv, "DB SCRIPT");
    return TCL_ERROR;
  }
  if( rtreeGetDbPointer(interp, Tcl_GetString(objv[1]), &db) ) return TCL_ERROR;

  pCtx = (BoxGeomCtx*)ckalloc(sizeof(BoxGeomCtx*));
  pCtx->interp = interp;
  pCtx->pScript = Tcl_DuplicateObj(objv[2]);
  Tcl_IncrRefCount(pCtx->pScript);

  sqlite3_rtree_geometry_callback(db, "box", box_geom, (void*)pCtx);

  sqlite3_snprintf(64, aPtr, "%p", (void*)pCtx);
  Tcl_SetObjResult(interp, Tcl_NewStringObj(aPtr, -1));
  return TCL_OK;
}

static int box_query(sqlite3_rtree_query_info *pInfo){
  const char *azParentWithin[] = {"not", "partly", "fully", 0};
  BoxQueryCtx *pCtx = (BoxQueryCtx*)pInfo->pContext;
  Tcl_Interp *interp = pCtx->interp;
  Tcl_Obj *pEval;
  Tcl_Obj *pArg;
  Tcl_Obj *pTmp = 0;
  int rc;
  int ii;

  pEval = Tcl_DuplicateObj(pCtx->pScript);
  Tcl_IncrRefCount(pEval);
  pArg = Tcl_NewObj();
  Tcl_IncrRefCount(pArg);

  /* aParam[] */
  pTmp = Tcl_NewObj();
  Tcl_IncrRefCount(pTmp);
  for(ii=0; ii<pInfo->nParam; ii++){
    Tcl_Obj *p = Tcl_NewDoubleObj(pInfo->aParam[ii]);
    Tcl_ListObjAppendElement(interp, pTmp, p);
  }
  Tcl_ListObjAppendElement(interp, pArg, Tcl_NewStringObj("aParam", -1));
  Tcl_ListObjAppendElement(interp, pArg, pTmp);
  Tcl_DecrRefCount(pTmp);

  /* aCoord[] */
  pTmp = Tcl_NewObj();
  Tcl_IncrRefCount(pTmp);
  for(ii=0; ii<pInfo->nCoord; ii++){
    Tcl_Obj *p = Tcl_NewDoubleObj(pInfo->aCoord[ii]);
    Tcl_ListObjAppendElement(interp, pTmp, p);
  }
  Tcl_ListObjAppendElement(interp, pArg, Tcl_NewStringObj("aCoord", -1));
  Tcl_ListObjAppendElement(interp, pArg, pTmp);
  Tcl_DecrRefCount(pTmp);

  /* anQueue[] */
  pTmp = Tcl_NewObj();
  Tcl_IncrRefCount(pTmp);
  for(ii=0; ii<=pInfo->mxLevel; ii++){
    Tcl_Obj *p = Tcl_NewIntObj((int)pInfo->anQueue[ii]);
    Tcl_ListObjAppendElement(interp, pTmp, p);
  }
  Tcl_ListObjAppendElement(interp, pArg, Tcl_NewStringObj("anQueue", -1));
  Tcl_ListObjAppendElement(interp, pArg, pTmp);
  Tcl_DecrRefCount(pTmp);
  
  /* iLevel */
  Tcl_ListObjAppendElement(interp, pArg, Tcl_NewStringObj("iLevel", -1));
  Tcl_ListObjAppendElement(interp, pArg, Tcl_NewIntObj(pInfo->iLevel));

  /* mxLevel */
  Tcl_ListObjAppendElement(interp, pArg, Tcl_NewStringObj("mxLevel", -1));
  Tcl_ListObjAppendElement(interp, pArg, Tcl_NewIntObj(pInfo->mxLevel));

  /* iRowid */
  Tcl_ListObjAppendElement(interp, pArg, Tcl_NewStringObj("iRowid", -1));
  Tcl_ListObjAppendElement(interp, pArg, Tcl_NewWideIntObj(pInfo->iRowid));

  /* rParentScore */
  Tcl_ListObjAppendElement(interp, pArg, Tcl_NewStringObj("rParentScore", -1));
  Tcl_ListObjAppendElement(interp, pArg, Tcl_NewDoubleObj(pInfo->rParentScore));

  /* eParentWithin */
  assert( pInfo->eParentWithin==0 
       || pInfo->eParentWithin==1 
       || pInfo->eParentWithin==2 
  );
  Tcl_ListObjAppendElement(interp, pArg, Tcl_NewStringObj("eParentWithin", -1));
  Tcl_ListObjAppendElement(interp, pArg, 
      Tcl_NewStringObj(azParentWithin[pInfo->eParentWithin], -1)
  );

  Tcl_ListObjAppendElement(interp, pEval, pArg);
  rc = Tcl_EvalObjEx(interp, pEval, 0) ? SQLITE_ERROR : SQLITE_OK;

  if( rc==SQLITE_OK ){
    double rScore = 0.0;
    Tcl_Size nObj = 0;
    int eP = 0;
    Tcl_Obj **aObj = 0;
    Tcl_Obj *pRes = Tcl_GetObjResult(interp);

    if( Tcl_ListObjGetElements(interp, pRes, &nObj, &aObj) 
     || nObj!=2 
     || Tcl_GetDoubleFromObj(interp, aObj[1], &rScore)
     || Tcl_GetIndexFromObj(interp, aObj[0], azParentWithin, "value", 0, &eP)
    ){
      rc = SQLITE_ERROR;
    }else{
      pInfo->rScore = rScore;
      pInfo->eParentWithin = eP;
    }
  }

  Tcl_DecrRefCount(pArg);
  Tcl_DecrRefCount(pEval);
  return rc;
}

static void box_query_destroy(void *p){
  BoxQueryCtx *pCtx = (BoxQueryCtx*)p;
  Tcl_DecrRefCount(pCtx->pScript);
  ckfree((char*)pCtx);
}

static int SQLITE_TCLAPI register_box_query(
  void * clientData,
  Tcl_Interp *interp,
  int objc,
  Tcl_Obj *CONST objv[]
){
  sqlite3 *db;
  BoxQueryCtx *pCtx;
  UNUSED_PARAMETER(clientData);

  if( objc!=3 ){
    Tcl_WrongNumArgs(interp, 1, objv, "DB SCRIPT");
    return TCL_ERROR;
  }
  if( rtreeGetDbPointer(interp, Tcl_GetString(objv[1]), &db) ) return TCL_ERROR;

  pCtx = (BoxQueryCtx*)ckalloc(sizeof(BoxQueryCtx));
  pCtx->interp = interp;
  pCtx->pScript = Tcl_DuplicateObj(objv[2]);
  Tcl_IncrRefCount(pCtx->pScript);

  sqlite3_rtree_query_callback(
      db, "qbox", box_query, (void*)pCtx, box_query_destroy
  );

  Tcl_ResetResult(interp);
  return TCL_OK;
}

/* 
** Type used to cache parameter information for the "circle" r-tree geometry
** callback.
*/
typedef struct Circle Circle;
struct Circle {
  struct Box {
    double xmin;
    double xmax;
    double ymin;
    double ymax;
  } aBox[2];
  double centerx;
  double centery;
  double radius;
  double mxArea;
  int eScoreType;
};

/*
** Destructor function for Circle objects allocated by circle_geom().
*/
static void circle_del(void *p){
  sqlite3_free(p);
}

/*
** Implementation of "circle" r-tree geometry callback.
*/
static int circle_geom(
  sqlite3_rtree_geometry *p,
  int nCoord, 
  sqlite3_rtree_dbl *aCoord,
  int *pRes
){
  int i;                          /* Iterator variable */
  Circle *pCircle;                /* Structure defining circular region */
  double xmin, xmax;              /* X dimensions of box being tested */
  double ymin, ymax;              /* X dimensions of box being tested */

  xmin = aCoord[0];
  xmax = aCoord[1];
  ymin = aCoord[2];
  ymax = aCoord[3];
  pCircle = (Circle *)p->pUser;
  if( pCircle==0 ){
    /* If pUser is still 0, then the parameter values have not been tested
    ** for correctness or stored into a Circle structure yet. Do this now. */

    /* This geometry callback is for use with a 2-dimensional r-tree table.
    ** Return an error if the table does not have exactly 2 dimensions. */
    if( nCoord!=4 ) return SQLITE_ERROR;

    /* Test that the correct number of parameters (3) have been supplied,
    ** and that the parameters are in range (that the radius of the circle 
    ** radius is greater than zero). */
    if( p->nParam!=3 || p->aParam[2]<0.0 ) return SQLITE_ERROR;

    /* Allocate a structure to cache parameter data in. Return SQLITE_NOMEM
    ** if the allocation fails. */
    pCircle = (Circle *)(p->pUser = sqlite3_malloc(sizeof(Circle)));
    if( !pCircle ) return SQLITE_NOMEM;
    p->xDelUser = circle_del;

    /* Record the center and radius of the circular region. One way that
    ** tested bounding boxes that intersect the circular region are detected
    ** is by testing if each corner of the bounding box lies within radius
    ** units of the center of the circle. */
    pCircle->centerx = p->aParam[0];
    pCircle->centery = p->aParam[1];
    pCircle->radius = p->aParam[2];

    /* Define two bounding box regions. The first, aBox[0], extends to
    ** infinity in the X dimension. It covers the same range of the Y dimension
    ** as the circular region. The second, aBox[1], extends to infinity in
    ** the Y dimension and is constrained to the range of the circle in the
    ** X dimension.
    **
    ** Then imagine each box is split in half along its short axis by a line
    ** that intersects the center of the circular region. A bounding box
    ** being tested can be said to intersect the circular region if it contains
    ** points from each half of either of the two infinite bounding boxes.
    */
    pCircle->aBox[0].xmin = pCircle->centerx;
    pCircle->aBox[0].xmax = pCircle->centerx;
    pCircle->aBox[0].ymin = pCircle->centery + pCircle->radius;
    pCircle->aBox[0].ymax = pCircle->centery - pCircle->radius;
    pCircle->aBox[1].xmin = pCircle->centerx + pCircle->radius;
    pCircle->aBox[1].xmax = pCircle->centerx - pCircle->radius;
    pCircle->aBox[1].ymin = pCircle->centery;
    pCircle->aBox[1].ymax = pCircle->centery;
    pCircle->mxArea = (xmax - xmin)*(ymax - ymin) + 1.0;
  }

  /* Check if any of the 4 corners of the bounding-box being tested lie 
  ** inside the circular region. If they do, then the bounding-box does
  ** intersect the region of interest. Set the output variable to true and
  ** return SQLITE_OK in this case. */
  for(i=0; i<4; i++){
    double x = (i&0x01) ? xmax : xmin;
    double y = (i&0x02) ? ymax : ymin;
    double d2;
    
    d2  = (x-pCircle->centerx)*(x-pCircle->centerx);
    d2 += (y-pCircle->centery)*(y-pCircle->centery);
    if( d2<(pCircle->radius*pCircle->radius) ){
      *pRes = 1;
      return SQLITE_OK;
    }
  }

  /* Check if the bounding box covers any other part of the circular region.
  ** See comments above for a description of how this test works. If it does
  ** cover part of the circular region, set the output variable to true
  ** and return SQLITE_OK. */
  for(i=0; i<2; i++){
    if( xmin<=pCircle->aBox[i].xmin 
     && xmax>=pCircle->aBox[i].xmax 
     && ymin<=pCircle->aBox[i].ymin 
     && ymax>=pCircle->aBox[i].ymax 
    ){
      *pRes = 1;
      return SQLITE_OK;
    }
  }

  /* The specified bounding box does not intersect the circular region. Set
  ** the output variable to zero and return SQLITE_OK. */
  *pRes = 0;
  return SQLITE_OK;
}

/*
** Implementation of "circle" r-tree geometry callback using the 
** 2nd-generation interface that allows scoring.
**
** Two calling forms:
**
**          Qcircle(X,Y,Radius,eType)        -- All values are doubles
**          Qcircle('x:X y:Y r:R e:ETYPE')   -- Single string parameter
*/
static int circle_query_func(sqlite3_rtree_query_info *p){
  int i;                          /* Iterator variable */
  Circle *pCircle;                /* Structure defining circular region */
  double xmin, xmax;              /* X dimensions of box being tested */
  double ymin, ymax;              /* X dimensions of box being tested */
  int nWithin = 0;                /* Number of corners inside the circle */

  xmin = p->aCoord[0];
  xmax = p->aCoord[1];
  ymin = p->aCoord[2];
  ymax = p->aCoord[3];
  pCircle = (Circle *)p->pUser;
  if( pCircle==0 ){
    /* If pUser is still 0, then the parameter values have not been tested
    ** for correctness or stored into a Circle structure yet. Do this now. */

    /* This geometry callback is for use with a 2-dimensional r-tree table.
    ** Return an error if the table does not have exactly 2 dimensions. */
    if( p->nCoord!=4 ) return SQLITE_ERROR;

    /* Test that the correct number of parameters (1 or 4) have been supplied.
    */
    if( p->nParam!=4 && p->nParam!=1 ) return SQLITE_ERROR;

    /* Allocate a structure to cache parameter data in. Return SQLITE_NOMEM
    ** if the allocation fails. */
    pCircle = (Circle *)(p->pUser = sqlite3_malloc(sizeof(Circle)));
    if( !pCircle ) return SQLITE_NOMEM;
    p->xDelUser = circle_del;

    /* Record the center and radius of the circular region. One way that
    ** tested bounding boxes that intersect the circular region are detected
    ** is by testing if each corner of the bounding box lies within radius
    ** units of the center of the circle. */
    if( p->nParam==4 ){
      pCircle->centerx = p->aParam[0];
      pCircle->centery = p->aParam[1];
      pCircle->radius = p->aParam[2];
      pCircle->eScoreType = (int)p->aParam[3];
    }else{
      const char *z = (const char*)sqlite3_value_text(p->apSqlParam[0]);
      pCircle->centerx = 0.0;
      pCircle->centery = 0.0;
      pCircle->radius = 0.0;
      pCircle->eScoreType = 0;
      while( z && z[0] ){
        if( z[0]=='r' && z[1]==':' ){
          pCircle->radius = atof(&z[2]);
        }else if( z[0]=='x' && z[1]==':' ){
          pCircle->centerx = atof(&z[2]);
        }else if( z[0]=='y' && z[1]==':' ){
          pCircle->centery = atof(&z[2]);
        }else if( z[0]=='e' && z[1]==':' ){
          pCircle->eScoreType = (int)atof(&z[2]);
        }else if( z[0]==' ' ){
          z++;
          continue;
        }
        while( z[0]!=0 && z[0]!=' ' ) z++;
        while( z[0]==' ' ) z++;
      }
    }
    if( pCircle->radius<0.0 ){
      sqlite3_free(pCircle);
      return SQLITE_NOMEM;
    }

    /* Define two bounding box regions. The first, aBox[0], extends to
    ** infinity in the X dimension. It covers the same range of the Y dimension
    ** as the circular region. The second, aBox[1], extends to infinity in
    ** the Y dimension and is constrained to the range of the circle in the
    ** X dimension.
    **
    ** Then imagine each box is split in half along its short axis by a line
    ** that intersects the center of the circular region. A bounding box
    ** being tested can be said to intersect the circular region if it contains
    ** points from each half of either of the two infinite bounding boxes.
    */
    pCircle->aBox[0].xmin = pCircle->centerx;
    pCircle->aBox[0].xmax = pCircle->centerx;
    pCircle->aBox[0].ymin = pCircle->centery + pCircle->radius;
    pCircle->aBox[0].ymax = pCircle->centery - pCircle->radius;
    pCircle->aBox[1].xmin = pCircle->centerx + pCircle->radius;
    pCircle->aBox[1].xmax = pCircle->centerx - pCircle->radius;
    pCircle->aBox[1].ymin = pCircle->centery;
    pCircle->aBox[1].ymax = pCircle->centery;
    pCircle->mxArea = 200.0*200.0;
  }

  /* Check if any of the 4 corners of the bounding-box being tested lie 
  ** inside the circular region. If they do, then the bounding-box does
  ** intersect the region of interest. Set the output variable to true and
  ** return SQLITE_OK in this case. */
  for(i=0; i<4; i++){
    double x = (i&0x01) ? xmax : xmin;
    double y = (i&0x02) ? ymax : ymin;
    double d2;
    
    d2  = (x-pCircle->centerx)*(x-pCircle->centerx);
    d2 += (y-pCircle->centery)*(y-pCircle->centery);
    if( d2<(pCircle->radius*pCircle->radius) ) nWithin++;
  }

  /* Check if the bounding box covers any other part of the circular region.
  ** See comments above for a description of how this test works. If it does
  ** cover part of the circular region, set the output variable to true
  ** and return SQLITE_OK. */
  if( nWithin==0 ){
    for(i=0; i<2; i++){
      if( xmin<=pCircle->aBox[i].xmin 
       && xmax>=pCircle->aBox[i].xmax 
       && ymin<=pCircle->aBox[i].ymin 
       && ymax>=pCircle->aBox[i].ymax 
      ){
        nWithin = 1;
        break;
      }
    }
  }

  if( pCircle->eScoreType==1 ){
    /* Depth first search */
    p->rScore = p->iLevel;
  }else if( pCircle->eScoreType==2 ){
    /* Breadth first search */
    p->rScore = 100 - p->iLevel;
  }else if( pCircle->eScoreType==3 ){
    /* Depth-first search, except sort the leaf nodes by area with
    ** the largest area first */
    if( p->iLevel==1 ){
      p->rScore = 1.0 - (xmax-xmin)*(ymax-ymin)/pCircle->mxArea;
      if( p->rScore<0.01 ) p->rScore = 0.01;
    }else{
      p->rScore = 0.0;
    }
  }else if( pCircle->eScoreType==4 ){
    /* Depth-first search, except exclude odd rowids */
    p->rScore = p->iLevel;
    if( p->iRowid&1 ) nWithin = 0;
  }else{
    /* Breadth-first search, except exclude odd rowids */
    p->rScore = 100 - p->iLevel;
    if( p->iRowid&1 ) nWithin = 0;
  }
  if( nWithin==0 ){
    p->eWithin = NOT_WITHIN;
  }else if( nWithin>=4 ){
    p->eWithin = FULLY_WITHIN;
  }else{
    p->eWithin = PARTLY_WITHIN;
  }
  return SQLITE_OK;
}
/*
** Implementation of "breadthfirstsearch" r-tree geometry callback using the 
** 2nd-generation interface that allows scoring.
**
**     ... WHERE id MATCH breadthfirstsearch($x0,$x1,$y0,$y1) ...
**
** It returns all entries whose bounding boxes overlap with $x0,$x1,$y0,$y1.
*/
static int bfs_query_func(sqlite3_rtree_query_info *p){
  double x0,x1,y0,y1;        /* Dimensions of box being tested */
  double bx0,bx1,by0,by1;    /* Boundary of the query function */

  if( p->nParam!=4 ) return SQLITE_ERROR;
  x0 = p->aCoord[0];
  x1 = p->aCoord[1];
  y0 = p->aCoord[2];
  y1 = p->aCoord[3];
  bx0 = p->aParam[0];
  bx1 = p->aParam[1];
  by0 = p->aParam[2];
  by1 = p->aParam[3];
  p->rScore = 100 - p->iLevel;
  if( p->eParentWithin==FULLY_WITHIN ){
    p->eWithin = FULLY_WITHIN;
  }else if( x0>=bx0 && x1<=bx1 && y0>=by0 && y1<=by1 ){
    p->eWithin = FULLY_WITHIN;
  }else if( x1>=bx0 && x0<=bx1 && y1>=by0 && y0<=by1 ){
    p->eWithin = PARTLY_WITHIN;
  }else{
    p->eWithin = NOT_WITHIN;
  }
  return SQLITE_OK;
}

/* END of implementation of "circle" geometry callback.
**************************************************************************
*************************************************************************/

#include <assert.h>
#include "tclsqlite.h"

typedef struct Cube Cube;
struct Cube {
  double x;
  double y;
  double z;
  double width;
  double height;
  double depth;
};

static void cube_context_free(void *p){
  sqlite3_free(p);
}

/*
** The context pointer registered along with the 'cube' callback is
** always ((void *)&gHere). This is just to facilitate testing, it is not
** actually used for anything.
*/
static int gHere = 42;

/*
** Implementation of a simple r-tree geom callback to test for intersection
** of r-tree rows with a "cube" shape. Cubes are defined by six scalar
** coordinates as follows:
**
**   cube(x, y, z, width, height, depth)
**
** The width, height and depth parameters must all be greater than zero.
*/
static int cube_geom(
  sqlite3_rtree_geometry *p,
  int nCoord,
  sqlite3_rtree_dbl *aCoord,
  int *piRes
){
  Cube *pCube = (Cube *)p->pUser;

  assert( p->pContext==(void *)&gHere );

  if( pCube==0 ){
    if( p->nParam!=6 || nCoord!=6
     || p->aParam[3]<=0.0 || p->aParam[4]<=0.0 || p->aParam[5]<=0.0
    ){
      return SQLITE_ERROR;
    }
    pCube = (Cube *)sqlite3_malloc(sizeof(Cube));
    if( !pCube ){
      return SQLITE_NOMEM;
    }
    pCube->x = p->aParam[0];
    pCube->y = p->aParam[1];
    pCube->z = p->aParam[2];
    pCube->width = p->aParam[3];
    pCube->height = p->aParam[4];
    pCube->depth = p->aParam[5];

    p->pUser = (void *)pCube;
    p->xDelUser = cube_context_free;
  }

  assert( nCoord==6 );
  *piRes = 0;
  if( aCoord[0]<=(pCube->x+pCube->width)
   && aCoord[1]>=pCube->x
   && aCoord[2]<=(pCube->y+pCube->height)
   && aCoord[3]>=pCube->y
   && aCoord[4]<=(pCube->z+pCube->depth)
   && aCoord[5]>=pCube->z
  ){
    *piRes = 1;
  }

  return SQLITE_OK;
}
#endif /* SQLITE_ENABLE_RTREE */

static int SQLITE_TCLAPI register_cube_geom(
  void * clientData,
  Tcl_Interp *interp,
  int objc,
  Tcl_Obj *CONST objv[]
){
  (void)clientData;
#ifndef SQLITE_ENABLE_RTREE
  UNUSED_PARAMETER(interp);
  UNUSED_PARAMETER(objc);
  UNUSED_PARAMETER(objv);
#else
  sqlite3 *db;
  int rc;

  if( objc!=2 ){
    Tcl_WrongNumArgs(interp, 1, objv, "DB");
    return TCL_ERROR;
  }
  if( rtreeGetDbPointer(interp, Tcl_GetString(objv[1]), &db) ) return TCL_ERROR;
  rc = sqlite3_rtree_geometry_callback(db, "cube", cube_geom, (void *)&gHere);
  Tcl_SetResult(interp, (char *)sqlite3ErrStr(rc), TCL_STATIC);
#endif
  return TCL_OK;
}

static int SQLITE_TCLAPI register_circle_geom(
  void * clientData,
  Tcl_Interp *interp,
  int objc,
  Tcl_Obj *CONST objv[]
){
  (void)clientData;
#ifndef SQLITE_ENABLE_RTREE
  UNUSED_PARAMETER(interp);
  UNUSED_PARAMETER(objc);
  UNUSED_PARAMETER(objv);
#else
  sqlite3 *db;
  int rc;

  if( objc!=2 ){
    Tcl_WrongNumArgs(interp, 1, objv, "DB");
    return TCL_ERROR;
  }
  if( rtreeGetDbPointer(interp, Tcl_GetString(objv[1]), &db) ) return TCL_ERROR;
  rc = sqlite3_rtree_geometry_callback(db, "circle", circle_geom, 0);
  if( rc==SQLITE_OK ){
    rc = sqlite3_rtree_query_callback(db, "Qcircle",
                                      circle_query_func, 0, 0);
  }
  if( rc==SQLITE_OK ){
    rc = sqlite3_rtree_query_callback(db, "breadthfirstsearch",
                                      bfs_query_func, 0, 0);
  }
  Tcl_SetResult(interp, (char *)sqlite3ErrStr(rc), TCL_STATIC);
#endif
  return TCL_OK;
}


int Sqlitetestrtreedoc_Init(Tcl_Interp *interp){
#ifndef SQLITE_ENABLE_RTREE
  UNUSED_PARAMETER(interp);
#else
  Tcl_CreateObjCommand(interp, "register_box_geom", register_box_geom, 0, 0);
  Tcl_CreateObjCommand(interp, "register_box_query", register_box_query, 0, 0);
  Tcl_CreateObjCommand(interp, "register_cube_geom", register_cube_geom, 0, 0);
  Tcl_CreateObjCommand(interp, "register_circle_geom",register_circle_geom,0,0);
#endif /* SQLITE_ENABLE_RTREE */
  return TCL_OK;
}
