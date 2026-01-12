rm -rf mqom2-github.zip
zip -r mqom2-github.zip release_github
ssh kraken.cryptoexperts.net -C "rm -rf mqom2-github"
scp mqom2-github.zip kraken.cryptoexperts.net:~/
ssh kraken.cryptoexperts.net -C "unzip mqom2-github.zip"
ssh kraken.cryptoexperts.net -C "rm mqom2-github.zip"
ssh kraken.cryptoexperts.net -C "mv release_github mqom2-github"
