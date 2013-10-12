package org.netxms.certificate.loader;

import java.io.IOException;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.security.NoSuchProviderException;
import java.security.cert.Certificate;
import java.security.cert.CertificateException;
import java.util.List;

public interface KeyStoreLoader
{
   KeyStore loadKeyStore()
      throws CertificateException, NoSuchAlgorithmException, NoSuchProviderException, KeyStoreException, IOException;
}
