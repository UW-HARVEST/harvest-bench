#include <tcl.h>
#include <ctype.h>
#define SQLITE_TCLAPI
static int SQLITE_TCLAPI fpnum_compare(
  void * clientData,
  Tcl_Interp *interp,
  int objc,
  Tcl_Obj *CONST objv[]
){
  const unsigned char *zA;
  const unsigned char *zB;
  int i, j;
  int nDigit;

  if( objc!=3 ){
    Tcl_WrongNumArgs(interp, 1, objv, "STRING1 STRING2");
    return TCL_ERROR;
  }
  zA = (const unsigned char*)Tcl_GetString(objv[1]);
  zB = (const unsigned char*)Tcl_GetString(objv[2]);
  i = j = 0;
  while( 1 ){
    /* Skip whitespace before and after tokens */
    while( isspace(zA[i]) ){ i++; }
    while( isspace(zB[j]) ){ j++; }

    if( zA[i]!=zB[j] ) break;                /* First character must match */
    if( zA[i]=='-' && isdigit(zA[i+1]) ){ i++; j++; }  /* Skip initial '-' */
    if( !isdigit(zA[i]) ){
      /* Not a number.  Must match exactly */
      while( !isspace(zA[i]) && zA[i] && zA[i]==zB[j] ){ i++; j++; }
      if( zA[i]!=zB[j] ) break;
      if( isspace(zA[i]) ) continue;
      break;
    }

    /* At this point we know we are dealing with a number zA[i] and zB[j]
    ** are both digits (leading "-" have been skipped).  See if they are
    ** the same number.  Start by matching digits before the decimal
    ** point, which must all be the same. */
    nDigit = 0;
    while( zA[i]==zB[j] && isdigit(zA[i]) ){ i++; j++; nDigit++; }
    if( zA[i]!=zB[j] ) break;
    if( zA[i]==0 ) break;
    if( zA[i]=='.' && zB[j]=='.' ){
      /* Count more matching digits after the decimal point */
      i++;
      j++;
      while( zA[i]==zB[j] && isdigit(zA[i]) ){ i++; j++; nDigit++; }
      if( zA[i]==0 ){
        while( zB[j]=='0' || (isdigit(zB[j]) && nDigit>=15) ){ j++; nDigit++; }
        break;
      }
      if( zB[j]==0 ){
        while( zA[i]=='0' || (isdigit(zA[i]) && nDigit>=15) ){ i++; nDigit++; }
        break;
      }
      if( isspace(zA[i]) && isspace(zB[j]) ) continue;

      if( isdigit(zA[i]) && isdigit(zB[j]) ){
        /* A and B are both digits, but different digits */
        if( zA[i]==zB[j]+1 && !isdigit(zA[i+1]) && isdigit(zB[j+1]) ){
          /* Is A a rounded up version of B? */
          j++;
          while( zB[j]=='9' ){ j++; nDigit++; }
          if( nDigit<14 && (!isdigit(zB[j]) || zB[j]<5) ) break;
          while( isdigit(zB[j]) ){ j++; }
          i++;
        }else if( zB[j]==zA[i]+1 && !isdigit(zB[j+1]) && isdigit(zA[i+1]) ){
          /* Is B a rounded up version of A? */
          i++;
          while( zA[i]=='9' ){ i++; nDigit++; }
          if( nDigit<14 && (!isdigit(zA[i]) || zA[i]<5) ) break;
          while( isdigit(zA[i]) ){ i++; }
          j++;
        }else{
          break;
        }
      }else if( !isdigit(zA[i]) && isdigit(zB[j]) ){
        while( zB[j]=='0' ){ j++; nDigit++; }
        if( nDigit<15 ) break;
        while( isdigit(zB[j]) ){ j++; }
      }else if( !isdigit(zB[j]) && isdigit(zA[i]) ){
        while( zA[i]=='0' ){ i++; nDigit++; }
        if( nDigit<15 ) break;
        while( isdigit(zA[i]) ){ i++; }
      }else{
        break;
      }
    }
    if( zA[i]=='e' && zB[j]=='e' ){
      i++;
      j++;
      if( (zA[i]=='+' || zA[i]=='-') && zB[j]==zA[i] ){  i++;  j++; }
      if( zA[i]!=zB[j] ){
        if( zA[i]=='0' && zA[i+1]==zB[j] ){ i++; }
        if( zB[j]=='0' && zB[j+1]==zA[i] ){ j++; }
      }
      while( zA[i]==zB[j] && isdigit(zA[i]) ){ i++; j++; }
      if( zA[i]!=zB[j] ) break;
      if( zA[i]==0 ) break;
      continue;
    }
  }
  while( isspace(zA[i]) ){ i++; }
  while( isspace(zB[j]) ){ j++; }  
  Tcl_SetObjResult(interp, Tcl_NewIntObj(zA[i]==0 && zB[j]==0));
  return TCL_OK;
}

static int SQLITE_TCLAPI f5tMayBeCorrupt(
  void * clientData,
  Tcl_Interp *interp,
  int objc,
  Tcl_Obj *CONST objv[]
){
#ifdef SQLITE_DEBUG
  int bOld = sqlite3_fts5_may_be_corrupt;

  if( objc!=2 && objc!=1 ){
    Tcl_WrongNumArgs(interp, 1, objv, "?BOOLEAN?");
    return TCL_ERROR;
  }
  if( objc==2 ){
    int bNew;
    if( Tcl_GetBooleanFromObj(interp, objv[1], &bNew) ) return TCL_ERROR;
    sqlite3_fts5_may_be_corrupt = bNew;
  }

  Tcl_SetObjResult(interp, Tcl_NewIntObj(bOld));
#endif
  return TCL_OK;
}

int Compare_Init(Tcl_Interp *i){Tcl_CreateObjCommand(i,"sqlite3_fts5_may_be_corrupt",f5tMayBeCorrupt,0,0);Tcl_CreateObjCommand(i,"fpnum_compare",fpnum_compare,0,0);return TCL_OK;}
