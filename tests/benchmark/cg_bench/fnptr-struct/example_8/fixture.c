/* CG-Bench fixture: fnptr-struct/example_8 */
/* fnptr: pkey->ameth->pkey_security_bits, targets: NULL */

int EVP_PKEY_security_bits(const EVP_PKEY *pkey)
{
    if (pkey == NULL)
        return 0;
    if (!pkey->ameth || !pkey->ameth->pkey_security_bits)
        return -2;
    return pkey->ameth->pkey_security_bits(pkey);
}

void EVP_PKEY_asn1_set_security_bits(EVP_PKEY_ASN1_METHOD *ameth,
                                     int (*pkey_security_bits) (const EVP_PKEY
                                                                *pk))
{
    ameth->pkey_security_bits = pkey_security_bits;
}


/* Stub implementation for NULL */
void NULL(void) {}
