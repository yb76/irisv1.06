cd build

cd os
..\ddl.exe -p%1 -f QC0008A4.MAN
cd ..

ddl -p%1 SponsorCert.crt Certif.crt

cd modem
rem ..\ddl.exe -p%1 ESN_aus.zip ESN_aus.p7s *MN=ESN_aus.zip
cd ..


ddl -p%1 *SMPW=1R1S *PW=1 *GO=f:auris.out -f iris.dld

exit
