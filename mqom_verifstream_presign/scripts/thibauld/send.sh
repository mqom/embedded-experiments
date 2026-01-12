rm -rf mqom2.zip
zip -r mqom2.zip ../../../mqom2_ref
ssh kraken.cryptoexperts.net -C "rm -rf mqom2"
scp mqom2.zip kraken.cryptoexperts.net:~/
ssh kraken.cryptoexperts.net -C "unzip mqom2.zip"
ssh kraken.cryptoexperts.net -C "rm mqom2.zip"
ssh kraken.cryptoexperts.net -C "mv mqom2_ref mqom2"
