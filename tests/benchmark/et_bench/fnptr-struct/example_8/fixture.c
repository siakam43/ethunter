/* ET-Bench fixture: fnptr-struct/example_8 */
/* Scenario: OpenSSL EVP_PKEY ASN1 method security bits dispatch.
   fnptr: pkey->ameth->pkey_security_bits
   targets: NULL (guarded by null check before call)
   caller: EVP_PKEY_security_bits */

#include <stddef.h>

typedef struct EVP_PKEY EVP_PKEY;
typedef struct EVP_PKEY_ASN1_METHOD EVP_PKEY_ASN1_METHOD;

struct EVP_PKEY_ASN1_METHOD {
    int (*pkey_security_bits)(const EVP_PKEY *pk);
    int (*pkey_public_check)(const EVP_PKEY *pk);
};

struct EVP_PKEY {
    EVP_PKEY_ASN1_METHOD *ameth;
};

/* Caller: checks pkey->ameth->pkey_security_bits != NULL then calls through struct */
int EVP_PKEY_security_bits(const EVP_PKEY *pkey)
{
    if (pkey == NULL)
        return 0;
    if (!pkey->ameth || !pkey->ameth->pkey_security_bits)
        return -2;
    return pkey->ameth->pkey_security_bits(pkey);
}

void EVP_PKEY_asn1_set_security_bits(EVP_PKEY_ASN1_METHOD *ameth,
                                     int (*pkey_security_bits)(const EVP_PKEY *pk))
{
    ameth->pkey_security_bits = pkey_security_bits;
}
