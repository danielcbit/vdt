# contact-parser.awk
#
# Objetivo: imprimir na saída padrão o total de bytes recebidos e o
# 	tempo total de contato entre dois nós.
#
# Para executar:
#	awk -f contact-parser.awk (arquivo_log) > (arquivo_resultado)
#

BEGIN {
	# Tamanho do vetor de índices
	size = 0;
	threshold = 15;
}

{
	indx = $1 "@" $2;
	# Se for o primeiro contato entre esses nós
	if (ttl_contact[indx] == 0) {
		ttl_contact[indx] = 1;
		new_indx = indx "@" ttl_contact[indx];

		# Este vetor armazena o total de bytes que o nó da coluna 1
		# recebeu do nó da segunda coluna. Estou considerando que a
		# comunicação (A, B) é diferente da comunicação (B, A).
		data_rcvd[new_indx] = data_rcvd[new_indx] + $4;

		# Este vetor armazena o tempo total de contato entre o nó da
		# coluna 1 e o da coluna 2. Considerei que os contatos
		# (A, B) e (B, A) são diferentes.
		time_ctct[new_indx] = time_ctct[new_indx] + $5;
	}
	else {
		# É o mesmo contato
		if (($3 - last_contact[indx]) < threshold) {
			new_indx = indx "@" ttl_contact[indx];
			data_rcvd[new_indx] = data_rcvd[new_indx] + $4;
			time_ctct[new_indx] = time_ctct[new_indx] + $5;
		}
		# Não é o mesmo contato
		else {
			ttl_contact[indx]++;
			new_indx = indx "@" ttl_contact[indx];
			data_rcvd[new_indx] = data_rcvd[new_indx] + $4;
			time_ctct[new_indx] = time_ctct[new_indx] + $5;
		}
	}
	last_contact[indx] = $3;

	# O vetor indx_array armazena todos os pares de ids que aparecem no
	# arquivo de log. Eu uso ele no final para imprimir as estatísticas
	# para cada par de ids.
	# O código abaixo faz uma busca sequencial para saber se o par de ids
	# corrente já existe no vetor indx_array. Se não tiver, ele será
	# inserido.
	found = 0;
	for (i = 1; i <= size; i++) {
		if (indx_array[i] == new_indx) {
			found = 1;
		}
	}
	if (found == 0) {
		size++;
		indx_array[size] = new_indx;
	}
}

END {
	# Para cada par de ids, eu vou imprimir o total de bytes recebidos
	# e o tempo total de contato.
	for (i = 1; i <= size; i++) {
		split(indx_array[i], node_id, "@");
		node1 = node_id[1];
		node2 = node_id[2];
		contact = node_id[3];
		printf ("%d %d %d %f %f\n", node1, node2, contact,
			data_rcvd[indx_array[i]], time_ctct[indx_array[i]]);
	}
}
