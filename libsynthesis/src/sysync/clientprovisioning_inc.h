/*
 *  File:         clientprovisioning_inc.h
 *
 */


// macro to declare clientprovisioning methods and fields
#define CLIENTPROVISIONING_CLASSDECL \
public: \
  /* - execute remote provisioning command */ \
  bool executeProvisioningString(const char *aProvisioningCmd, sInt32 &aActiveProfile);


/* eof */
