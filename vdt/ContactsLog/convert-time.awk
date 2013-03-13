# convert-time.awk
#
# Objetivo: transformar o tempo de início de contato, que está no formato
# 	hora:min:seg, em um formato numérico que possibilite ordená-lo.
#
#	Este código considera que todos os tempos armazenados no arquivo
#	de log referem-se ao mesmo dia.
#
# Para executar:
#       awk -f convert-time.awk (arquivo_log) > (arquivo_resultado)
#

{
	split($3, time, ":");
	new_time = (time[1] * 3600) + (time[2] * 60) + (time[3]);
	print $1, $2, new_time, $4, $5, $6, $7;
}
