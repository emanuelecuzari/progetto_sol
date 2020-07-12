#!/bin/bash 

#attesa terminazione processo supermercato
pid=$( pidof supermercato )
tail --pid=$pid -f /dev/null

#controllo esistenza file
if [ ! -e  logfile_name.txt ]; then
    echo "File con nome del log non trovato"
    exit -1
fi

statlog=$( cat logfile_name.txt )

#controllo esistenza del file di log
if [ ! -e  $statlog ]; then 
    echo "File di log non trovato"
    exit -1
fi

echo "FORMATO CLIENTI"
printf "| %s | %s | %s | %s | %s | %s |\n" "id" "prodotti_acquistati" "t_supermercato" "t_in_coda" "cambi" "cassa_scelta"
echo "FORMATO CASSE"
printf "| %s | %s | %s | %s | %s | %s |\n" "id" "prodotti_elaborati" "clienti_serviti" "t_tot_apertura" "t_medio_servizio" "chiusure"

count=0

#leggo dal file di log
while read -r line; do

    IFS=':' token=( $line )
    count=$(( count + 1 ))
    
    if [ "${token[0]}" == "CLIENTE" ]; then
        t_market=$( echo "scale=3; ${token[2]} / 1" | bc)
        t_coda=$( echo "scale=3; ${token[3]} / 1" | bc)
        printf "| %4d | %4d | %6s | %6s | %4d | %4d |\n" ${token[1]} ${token[5]} $t_market $t_coda ${token[4]} ${token[6]}
        continue
    fi

    if [ "${token[0]}" == "CASSA" ]; then
        t_tot=$( echo "scale=3; ${token[4]} / 1" | bc)
        myid=${token[1]}
        n_clienti=${token[2]}
        n_prod=${token[3]}
        chiusure=${token[5]}
        continue
    fi

    if [ "${token[0]}" == "TEMPI DI SERVIZIO" ]; then
        t_servizio=0
        continue
    fi

    if [ "${token[0]}" == "STOP" ]; then
        if [ $n_clienti -ne 0 ]; then
            t_medio=$( echo "scale=3; $t_servizio / $n_clienti" | bc)
        else 
            t_medio=0
        fi
        printf "| %4d | %4d | %6d | %6s | %4s | %4d |\n" $myid $n_prod $n_clienti $t_tot $t_medio $chiusure
        continue
    fi

    if [ "${token[0]}" == "SUPERMERCATO" ]; then
        echo "DATI SUPERMERCATO"
        echo "NUMERO CLIENTI: ${token[1]}"
        echo "NUMERO PRODOTTI ACQUISTATI: ${token[2]}"
        continue
    fi

    if [ -n $( echo $line | grep "[0-9]\." ) ]; then
        t_servizio=$( echo "$t_servizio + ${token[0]}" | bc )
        continue
    fi

done < $statlog

if [ $count -eq 0 ]; then
    echo "File vuoto"
    exit -1
fi
exit 0
