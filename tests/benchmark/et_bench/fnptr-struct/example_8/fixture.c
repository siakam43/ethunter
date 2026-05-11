/* ET-Bench fixture: fnptr-struct/example_8 */
/* Scenario: OpenSSL EVP_PKEY ASN1 method security bits dispatch.
   fnptr: pkey->ameth->pkey_security_bits
   targets: rsa_pkey_security_bits (wired via bind_asn1_methods)
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

/* Target: RSA-specific security bits implementation */
static int rsa_pkey_security_bits(const EVP_PKEY *pk)
{
    (void)pk;
    return 2048; /* RSA default key size */
}

/* Binding: wire RSA method to the ASN1 vtable */
void bind_asn1_methods(EVP_PKEY_ASN1_METHOD *ameth)
{
    EVP_PKEY_asn1_set_security_bits(ameth, rsa_pkey_security_bits);
}
