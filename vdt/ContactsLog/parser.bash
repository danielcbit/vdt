#!/bin/bash

# Testando se o nome do arquivo de log foi passado por parâmetro
if [ -z $1 -o -z $2]
then
	echo "Erro na passagem de parametros"
	echo "Como executar: ./parser.bash <arquivo de log> <arquivo de saida>"
	exit 0
fi

LOG_FILE=$1
OUT_FILE=$2

# Convertendo o tempo de início de contato em um valor que fica mais fácil
# de ser ordenado.
# Esse script considera que não há mudança de dia.
awk -f convert-time.awk $LOG_FILE > new-log-file.txt

# Ordenando o arquivo de log baseado no tempo de início de contato
sort -n -k 3 new-log-file.txt > sorted-log-file.txt

# Grava no arquivo results.txt o total de bytes recebidos e o
# tempo total de contato entre dois nós. Considera-se que houve um
# novo contato quando o tempo de início do último contato entre os
# dois nós foi maior que 15 segundos.
# O arquivo results.txt será deletado a cada execução desse script.
awk -f contact-parser.awk sorted-log-file.txt > $OUT_FILE

# Removendo os arquivos auxiliares
rm new-log-file.txt sorted-log-file.txt
