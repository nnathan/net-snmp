/*******************************
 *
 *	net-snmp/var_api.h
 *
 *	Net-SNMP library - Variable-handling public interface
 *
 *******************************/

#ifndef _NET_SNMP_VAR_API_H
#define _NET_SNMP_VAR_API_H

#include <stdio.h>
#include <net-snmp/struct.h>
#include <net-snmp/utils.h>
#include <net-snmp/types.h>


	/* OID-related routines */

netsnmp_oid var_create_oid(       void                  );
netsnmp_oid var_create_oid_name(  char   *name          );
netsnmp_oid var_create_oid_value( u_int  *name, int len );
netsnmp_oid var_copy_oid(      netsnmp_oid oid                        );
int         var_set_oid(       netsnmp_oid oid, char   *name          );
int         var_set_oid_value( netsnmp_oid oid, u_int  *name, int len );
void        var_free_oid(      netsnmp_oid oid                        );

int         var_bprint_oid( netsnmp_buf buf,    netsnmp_oid oid );
char*       var_sprint_oid( char *buf, int len, netsnmp_oid oid );
void        var_fprint_oid( FILE *fp,           netsnmp_oid oid );
void        var_print_oid(                      netsnmp_oid oid );


	/* Value-related routines */

netsnmp_value var_create_value( void                                              );
netsnmp_value var_create_set_value(                  char *val, int len, int type );
netsnmp_value var_copy_value(   netsnmp_value value                               );
int           var_set_value(    netsnmp_value value, char *val, int len, int type );
void          var_free_value(   netsnmp_value value                               );

int   var_bprint_value( netsnmp_buf buf,        netsnmp_value value, netsnmp_mib mib );
char *var_sprint_value( char *str_buf, int len, netsnmp_value value, netsnmp_mib mib );
void  var_fprint_value( FILE *fp,               netsnmp_value value, netsnmp_mib mib );
void  var_print_value(                          netsnmp_value value, netsnmp_mib mib );


	/* Varbind-related routines */

netsnmp_varbind var_create_varbind( void                                                   );
netsnmp_varbind var_create_set_varbind(               netsnmp_oid oid, netsnmp_value value );
netsnmp_varbind var_copy_varbind( netsnmp_varbind vb                                       );
int             var_set_varbind(  netsnmp_varbind vb, netsnmp_oid oid, netsnmp_value value );
void            var_free_varbind( netsnmp_varbind vb                                       );

int   var_bprint_varbind( netsnmp_buf buf,        netsnmp_varbind varbind );
char *var_sprint_varbind( char *str_buf, int len, netsnmp_varbind varbind );
void  var_fprint_varbind( FILE *fp,               netsnmp_varbind varbind );
void  var_print_varbind(                          netsnmp_varbind varbind );

#endif /* _NET_SNMP_VAR_API_H */
