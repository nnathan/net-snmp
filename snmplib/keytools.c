/*
 * keytools.c
 *
 * FIX	Decide how to publicize simple (currently internal) hash functions
 *	from KMT.  Otherwise they must be pulled from the package directly.
 */

#include <config.h>

#include <sys/types.h>
#if HAVE_WINSOCK_H
#include <winsock.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "asn1.h"
#include "snmp_api.h"
#ifdef USE_INTERNAL_MD5
#include "md5.h"
#endif

#include "scapi.h"
#include "keytools.h"
#include "tools.h"

#include "transform_oids.h"

int (*kmt_hash) (
	const int	  mode,		void  	 **context,
	const u_int8_t	 *data,		const int  data_len,     
	u_int8_t	**digest,	int	  *digest_len);

/*******************************************************************-o-******
 * generate_Ku
 *
 * Parameters:
 *	*hashtype	MIB OID for the transform type for hashing.
 *	 hashtype_len	Length of OID value.
 *	*P		Pre-allocated bytes of passpharase.
 *	 pplen		Length of passphrase.
 *	*Ku		Buffer to contain Ku.
 *	*kulen		Length of Ku buffer.
 *      
 * Returns:
 *	SNMPERR_SUCCESS			Success.
 *	SNMPERR_GENERR			All errors, including KMT errs.
 *	SNMPERR_KT_NOT_AVAILABLE	When kmt_hash cannot be instantiated.
 *
 *
 * Convert a passphrase into a master user key, Ku, according to the
 * algorithm given in RFC 2274 concerning the SNMPv3 User Security Model (USM)
 * as follows:
 *
 * Expand the passphrase to fill the passphrase buffer space, if necessary,
 * concatenation as many duplicates as possible of P to itself.  If P is
 * larger than the buffer space, truncate it to fit.
 *
 * Then hash the result with the given hashtype transform.  Return
 * the result as Ku.
 *
 * If successful, kulen contains the size of the hash written to Ku.
 *
 * NOTE  Passphrases less than USM_LENGTH_P_MIN characters in length
 *	 cause an error to be returned.
 *	 (Punt this check to the cmdline apps?  XXX)
 *
 * XXX	Should there be an option to store Ku in the KMT cache?  (!)
 */
int
generate_Ku(	oid	*hashtype,	u_int  hashtype_len,
		u_char	*P,		size_t  pplen,
		u_char	*Ku,		size_t *kulen)
#if defined(HAVE_LIBKMT) || defined(USE_INTERNAL_MD5)
{
	int		 rval   = SNMPERR_SUCCESS,
			 nbytes = USM_LENGTH_EXPANDED_PASSPHRASE;

        u_int            i, pindex = 0;
        int		 transform;

	u_char		 buf[USM_LENGTH_KU_HASHBLOCK],
			*bufp;

#ifdef HAVE_LIBKMT
	void		*context = NULL;
#else
        MDstruct         MD;
#endif
        
	/*
	 * Sanity check.
	 */
	if ( !hashtype || !P || !Ku || !kulen
		|| (*kulen<=0)
		|| (hashtype_len != USM_LENGTH_OID_TRANSFORM) )
	{
		QUITFUN(SNMPERR_GENERR, generate_Ku_quit);
	}

        if (pplen < USM_LENGTH_P_MIN) {
#ifdef SNMP_TESTING_CODE
          fprintf(stderr, "Warning: passphrase chosen is below the length requiremnts of the USM.\n");
#else
          snmp_set_detail(strdup("Password length too short."));
          QUITFUN(SNMPERR_GENERR, generate_Ku_quit);
#endif
        }


	/*
	 * Setup for the transform type.
	 */
        transform = sc_get_transform_type(hashtype, hashtype_len, &kmt_hash);
        if (transform == SNMPERR_GENERR)
          QUITFUN(SNMPERR_GENERR, generate_Ku_quit);

	/*
	 * Expand passphrase and reduce it to a hash.
	 */
#ifdef HAVE_LIBKMT
        rval = kmt_hash(KMT_CRYPT_MODE_INIT, &context, NULL, 0, NULL, NULL);
        QUITFUN(rval, generate_Ku_quit);
#else
        MDbegin(&MD);
#endif

        while (nbytes > 0) {
                bufp = buf;
                for (i = 0; i < USM_LENGTH_KU_HASHBLOCK; i++) {
                        *bufp++ = P[pindex++ % pplen];
                }

#ifdef HAVE_LIBKMT
                rval = kmt_hash(KMT_CRYPT_MODE_UPDATE,
                                &context,
                                buf,    USM_LENGTH_KU_HASHBLOCK,
                                NULL,   NULL);
                QUITFUN(rval, generate_Ku_quit);
#else
                MDupdate(&MD, buf, USM_LENGTH_KU_HASHBLOCK*8);
#endif

                nbytes -= USM_LENGTH_KU_HASHBLOCK;
        }

#ifdef HAVE_LIBKMT
        rval = kmt_hash(KMT_CRYPT_MODE_FINAL, &context, NULL, 0, &Ku, kulen);
	QUITFUN(rval, generate_Ku_quit);
#else
        MDupdate(&MD, buf, 0);
        *kulen = sc_get_properlength(hashtype, hashtype_len);
        MDget(&MD, Ku, *kulen);
#endif


#ifdef SNMP_TESTING_CODE
        DEBUGMSGTL(("generate_Ku", "generating Ku (from %s): ", P));
        for(i=0; i < *kulen; i++)
          DEBUGMSG(("generate_Ku", "%02x",Ku[i]));
        DEBUGMSG(("keytools","\n"));
#endif /* SNMP_TESTING_CODE */


generate_Ku_quit:
	memset(buf, 0, USM_LENGTH_KU_HASHBLOCK);
#ifdef HAVE_LIBKMT
        SNMP_FREE(context);
#endif
	return rval;

}  /* end generate_Ku() */

#else
_KEYTOOLS_NOT_AVAILABLE
#endif							/* HAVE_LIBKMT */




/*******************************************************************-o-******
 * generate_kul
 *
 * Parameters:
 *	*hashtype
 *	 hashtype_len
 *	*engineID
 *	 engineID_len
 *	*Ku		Master key for a given user.
 *	 ku_len		Length of Ku in bytes.
 *	*Kul		Localized key for a given user at engineID.
 *	*kul_len	Length of Kul buffer (IN); Length of Kul key (OUT).
 *      
 * Returns:
 *	SNMPERR_SUCCESS			Success.
 *	SNMPERR_GENERR			All errors, including KMT errs.
 *	SNMPERR_KT_NOT_AVAILABLE	When kmt_hash cannot be instantiated.
 *
 *
 * Ku MUST be the proper length (currently fixed) for the given hashtype.
 *
 * Upon successful return, Kul contains the localized form of Ku at
 * engineID, and the length of the key is stored in kul_len.
 *
 * The localized key method is defined in RFC2274, Sections 2.6 and A.2, and
 * originally documented in:
 *  	U. Blumenthal, N. C. Hien, B. Wijnen,
 *     	"Key Derivation for Network Management Applications",
 *	IEEE Network Magazine, April/May issue, 1997.
 *
 *
 * ASSUMES  SNMP_MAXBUF >= sizeof(Ku + engineID + Ku).
 *
 * NOTE  Localized keys for privacy transforms are generated via
 *	 the authentication transform held by the same usmUser.
 *
 * XXX	An engineID of any length is accepted, even if larger than
 *	what is spec'ed for the textual convention.
 */
int
generate_kul(	oid	*hashtype,	u_int  hashtype_len,
		u_char	*engineID,	size_t  engineID_len,
		u_char	*Ku,		size_t  ku_len,
		u_char	*Kul,		size_t *kul_len)
#if defined(HAVE_LIBKMT) || defined(USE_INTERNAL_MD5)
{
	int		 rval    = SNMPERR_SUCCESS;
	u_int		 nbytes  = 0;
        size_t           properlength;

	u_char		 buf[SNMP_MAXBUF];
	void		*context = NULL;
#ifdef SNMP_TESTING_CODE
        int		 i;
#endif


	/*
	 * Sanity check.
	 */
	if ( !hashtype || !engineID || !Ku || !Kul || !kul_len
		|| (engineID_len<=0) || (ku_len<=0) || (*kul_len<=0)
		|| (hashtype_len != USM_LENGTH_OID_TRANSFORM) )
	{
		QUITFUN(SNMPERR_GENERR, generate_kul_quit);
	}


        properlength = sc_get_properlength(hashtype, hashtype_len);
        if (properlength == SNMPERR_GENERR)
          QUITFUN(SNMPERR_GENERR, generate_kul_quit);
       

	if (((int)*kul_len < properlength) || ((int)ku_len < properlength) ) {
		QUITFUN(SNMPERR_GENERR, generate_kul_quit);
	}

	/*
	 * Concatenate Ku and engineID properly, then hash the result.
	 * Store it in Kul.
	 */
	nbytes = 0;
	memcpy(buf,	   Ku,		properlength); nbytes += properlength;
	memcpy(buf+nbytes, engineID,	engineID_len); nbytes += engineID_len;
	memcpy(buf+nbytes, Ku,		properlength); nbytes += properlength;

	rval = sc_hash(hashtype, hashtype_len, buf, nbytes, Kul, kul_len);

#ifdef SNMP_TESTING_CODE
        DEBUGMSGTL(("generate_kul", "generating Kul (from Ku): "));
        for(i=0; i < *kul_len; i++)
          DEBUGMSG(("generate_kul", "%02x",Kul[i]));
        DEBUGMSG(("generate_kul", "keytools\n"));
#endif /* SNMP_TESTING_CODE */

	QUITFUN(rval, generate_kul_quit);
		

generate_kul_quit:
	SNMP_FREE(context);
	return rval;

}  /* end generate_kul() */

#else
_KEYTOOLS_NOT_AVAILABLE
#endif							/* HAVE_LIBKMT */




/*******************************************************************-o-******
 * encode_keychange
 *
 * Parameters:
 *	*hashtype	MIB OID for the hash transform type.
 *	 hashtype_len	Length of the MIB OID hash transform type.
 *	*oldkey		Old key that is used to encodes the new key.
 *	 oldkey_len	Length of oldkey in bytes.
 *	*newkey		New key that is encoded using the old key.
 *	 newkey_len	Length of new key in bytes.
 *	*kcstring	Buffer to contain the KeyChange TC string.
 *	*kcstring_len	Length of kcstring buffer.
 *      
 * Returns:
 *	SNMPERR_SUCCESS			Success.
 *	SNMPERR_GENERR			All errors, including KMT errs.
 *	SNMPERR_KT_NOT_AVAILABLE	When kmt_hash cannot be instantiated.
 *
 *
 * Uses oldkey and acquired random bytes to encode newkey into kcstring
 * according to the rules of the KeyChange TC described in RFC 2274, Section 5.
 *
 * Upon successful return, *kcstring_len contains the length of the
 * encoded string.
 *
 * ASSUMES	Old and new key are always equal to each other, although
 *		this may be less than the transform type hash output
 * 		output length (eg, using KeyChange for a DESPriv key when
 *		the user also uses SHA1Auth).  This also implies that the
 *		hash placed in the second 1/2 of the key change string
 *		will be truncated before the XOR'ing when the hash output is 
 *		larger than that 1/2 of the key change string.
 *
 *		*kcstring_len will be returned as exactly twice that same
 *		length though the input buffer may be larger.
 *
 * XXX FIX:     Does not handle varibable length keys.
 * XXX FIX:     Does not handle keys larger than the hash algorithm used.
 */
int
encode_keychange(	oid	*hashtype,	u_int  hashtype_len,
			u_char	*oldkey,	size_t  oldkey_len,
			u_char	*newkey,	size_t  newkey_len,
			u_char	*kcstring,	size_t *kcstring_len)
#if defined(HAVE_LIBKMT) || defined(USE_INTERNAL_MD5)
{
	int		 rval    = SNMPERR_SUCCESS;
	size_t		 properlength;
        size_t            nbytes  = 0;

        u_char          *tmpbuf = NULL;
	void		*context = NULL;



	/*
	 * Sanity check.
	 */
	if ( !hashtype || !oldkey || !newkey || !kcstring || !kcstring_len
		|| (oldkey_len<=0) || (newkey_len<=0) || (*kcstring_len<=0)
		|| (hashtype_len != USM_LENGTH_OID_TRANSFORM) )
	{
		QUITFUN(SNMPERR_GENERR, encode_keychange_quit);
	}

	/*
	 * Setup for the transform type.
	 */
        properlength = sc_get_properlength(hashtype, hashtype_len);
        if (properlength == SNMPERR_GENERR)
          QUITFUN(SNMPERR_GENERR, encode_keychange_quit);

	if ( (oldkey_len != newkey_len) || (*kcstring_len < (2*oldkey_len)) )
	{
		QUITFUN(SNMPERR_GENERR, encode_keychange_quit);
	}

	properlength = SNMP_MIN((int)oldkey_len, properlength);

	/*
	 * Use the old key and some random bytes to encode the new key
	 * in the KeyChange TC format:
	 *	. Get random bytes (store in first half of kcstring),
	 *	. Hash (oldkey | random_bytes) (into second half of kcstring),
	 *	. XOR hash and newkey (into second half of kcstring).
	 *
	 * Getting the wrong number of random bytes is considered an error.
	 */
	nbytes = properlength;

#if defined(SNMP_TESTING_CODE) && defined(RANDOMZEROS)
		memset(kcstring, 0, nbytes);
		DEBUGMSG(("encode_keychange",
                          "** Using all zero bits for \"random\" delta of )"
                          "the keychange string! **\n"));
#else /* !SNMP_TESTING_CODE */
		rval = sc_random(kcstring, &nbytes);
		QUITFUN(rval, encode_keychange_quit);
		if ((int)nbytes != properlength) {
			QUITFUN(SNMPERR_GENERR, encode_keychange_quit);
		}
#endif /* !SNMP_TESTING_CODE */

        tmpbuf = (u_char *)malloc(properlength*2);
        memcpy(tmpbuf, oldkey, properlength);
        memcpy(tmpbuf+properlength, kcstring, properlength);

        *kcstring_len -= properlength;
        rval = sc_hash(hashtype, hashtype_len, tmpbuf, properlength*2,
                       kcstring+properlength, kcstring_len);
        
	QUITFUN(rval, encode_keychange_quit);

	*kcstring_len = (properlength*2);

	kcstring += properlength;
	nbytes    = 0;
	while ((int)(nbytes++) < properlength) {
		*kcstring++ = *kcstring ^ *newkey++;
	}

encode_keychange_quit:
	if (rval != SNMPERR_SUCCESS) memset(kcstring, 0, *kcstring_len);
        if (tmpbuf != NULL) SNMP_FREE(tmpbuf);
	SNMP_FREE(context);

	return rval;

}  /* end encode_keychange() */

#else
_KEYTOOLS_NOT_AVAILABLE
#endif							/* HAVE_LIBKMT */




/*******************************************************************-o-******
 * decode_keychange
 *
 * Parameters:
 *	*hashtype	MIB OID of the hash transform to use.
 *	 hashtype_len	Length of the hash transform MIB OID.
 *	*oldkey		Old key that is used to encode the new key.
 *	 oldkey_len	Length of oldkey in bytes.
 *	*kcstring	Encoded KeyString buffer containing the new key.
 *	 kcstring_len	Length of kcstring in bytes.
 *	*newkey		Buffer to hold the extracted new key.
 *	*newkey_len	Length of newkey in bytes.
 *      
 * Returns:
 *	SNMPERR_SUCCESS			Success.
 *	SNMPERR_GENERR			All errors, including KMT errs.
 *	SNMPERR_KT_NOT_AVAILABLE	When kmt_hash cannot be instantiated.
 *
 *
 * Decodes a string of bits encoded according to the KeyChange TC described
 * in RFC 2274, Section 5.  The new key is extracted from *kcstring with
 * the aid of the old key.
 *
 * Upon successful return, *newkey_len contains the length of the new key.
 *
 *
 * ASSUMES	Old key is exactly 1/2 the length of the KeyChange buffer,
 *		although this length may be less than the hash transform
 *		output.  Thus the new key length will be equal to the old
 *		key length.
 */

/* XXX:  if the newkey is not long enough, it should be freed and remalloced */
int
decode_keychange(	oid	*hashtype,	u_int  hashtype_len,
			u_char	*oldkey,	size_t  oldkey_len,
			u_char	*kcstring,	size_t  kcstring_len,
			u_char	*newkey,	size_t *newkey_len)
#if defined(HAVE_LIBKMT) || defined(USE_INTERNAL_MD5)
{
	int		 rval    = SNMPERR_SUCCESS;
	size_t		 properlength = 0;
	u_int		 nbytes  = 0;

	u_int8_t	*bufp,
			 tmp_buf[SNMP_MAXBUF];
        size_t           tmp_buf_len = SNMP_MAXBUF;
	void		*context = NULL;
        u_char          *tmpbuf = NULL;



	/*
	 * Sanity check.
	 */
	if ( !hashtype || !oldkey || !kcstring || !newkey || !newkey_len
		|| (oldkey_len<=0) || (kcstring_len<=0) || (*newkey_len<=0)
		|| (hashtype_len != USM_LENGTH_OID_TRANSFORM) )
	{
		QUITFUN(SNMPERR_GENERR, decode_keychange_quit);
	}


	/*
	 * Setup for the transform type.
	 */
        properlength = sc_get_properlength(hashtype, hashtype_len);
        if (properlength == SNMPERR_GENERR)
          QUITFUN(SNMPERR_GENERR, decode_keychange_quit);


	if ( ((oldkey_len*2) != kcstring_len) || (*newkey_len < oldkey_len) )
	{
		QUITFUN(SNMPERR_GENERR, decode_keychange_quit);
	}

	properlength = oldkey_len;
        *newkey_len = properlength;

	/*
	 * Use the old key and the given KeyChange TC string to recover
	 * the new key:
	 *	. Hash (oldkey | random_bytes) (into newkey),
	 *	. XOR hash and encoded (second) half of kcstring (into newkey).
	 */
        tmpbuf = (u_char *)malloc(properlength*2);
        memcpy(tmpbuf, oldkey, properlength);
        memcpy(tmpbuf+properlength, kcstring, properlength);

        rval = sc_hash(hashtype, hashtype_len, tmpbuf, properlength*2,
                       tmp_buf, &tmp_buf_len);
	QUITFUN(rval, decode_keychange_quit);

        memcpy(newkey, tmp_buf, properlength);
	bufp   = kcstring+properlength;
	nbytes = 0;
	while ((int)(nbytes++) < properlength) {
		*newkey++ = *newkey ^ *bufp++;
	}

decode_keychange_quit:
	if (rval != SNMPERR_SUCCESS) {
		memset(newkey, 0, properlength);
	}
	memset(tmp_buf, 0, SNMP_MAXBUF);
	SNMP_FREE(context);
        if (tmpbuf != NULL) SNMP_FREE(tmpbuf);

	return rval;

}  /* end decode_keychange() */

#else
_KEYTOOLS_NOT_AVAILABLE
#endif							/* HAVE_LIBKMT */

