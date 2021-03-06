#include "Process_Read.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>


gz_files fps;

R_buffer RDB;
static uint64_t total_reads;

pthread_mutex_t i_readinputMutex;
pthread_mutex_t i_queueMutex;
pthread_mutex_t i_terminateMutex;
pthread_cond_t i_flushCond;
pthread_cond_t i_readinputflushCond;
pthread_cond_t i_stallCond;
pthread_cond_t i_readinputstallCond;
pthread_mutex_t i_doneMutex;


uint8_t seq_nt6_table[256] = {
    5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 0, 5, 1,  5, 5, 5, 2,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 5, 5, 5,  3, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 0, 5, 1,  5, 5, 5, 2,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 5, 5, 5,  3, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
    5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5
};

char bit_t_seq_table[256][4] = {{0}};
char bit_t_seq_table_rc[256][4] = {{0}};
char s_H[5] = {'A', 'C', 'G', 'T', 'N'};
char rc_Table[5] = {'T', 'G', 'C', 'A', 'N'};


void init_All_reads(All_reads* r)
{
	r->index_size = READ_INIT_NUMBER;
	r->read_length = (uint64_t*)malloc(sizeof(uint64_t)*r->index_size);
	r->read_sperate = NULL;
	r->N_site = NULL;
	r->total_reads_bases = 0;
	r->name_index_size = READ_INIT_NUMBER;
	r->name_index = (uint64_t*)malloc(sizeof(uint64_t)*r->name_index_size);
	r->name_index[0] = 0;
	r->name = NULL;
	r->total_name_length = 0;
	r->total_reads = 0;
}

void destory_All_reads(All_reads* r)
{
	uint64_t i = 0;
	for (i = 0; i < r->total_reads; i++)
	{
		if (r->N_site[i] != NULL)
		{
			free(r->N_site[i]);
		}
		free(r->read_sperate[i]);
	}
	free(r->N_site);
	free(r->read_sperate);
	free(r->name);
	free(r->name_index);
	free(r->read_length);
}


void write_All_reads(All_reads* r, char* read_file_name)
{
    fprintf(stderr, "Writing reads to disk... \n");
    char* index_name = (char*)malloc(strlen(read_file_name)+15);
    sprintf(index_name, "%s.bin", read_file_name);
    FILE* fp = fopen(index_name, "w");
	fwrite(&asm_opt.adapterLen, sizeof(asm_opt.adapterLen), 1, fp);
    fwrite(&r->index_size, sizeof(r->index_size), 1, fp);
	fwrite(&r->name_index_size, sizeof(r->name_index_size), 1, fp);
	fwrite(&r->total_reads, sizeof(r->total_reads), 1, fp);
	fwrite(&r->total_reads_bases, sizeof(r->total_reads_bases), 1, fp);
	fwrite(&r->total_name_length, sizeof(r->total_name_length), 1, fp);

	uint64_t i = 0;
	uint64_t zero = 0;
	for (i = 0; i < r->total_reads; i++)
	{
		if (r->N_site[i] != NULL)
		{
			///number of Ns
			fwrite(&r->N_site[i][0], sizeof(r->N_site[i][0]), 1, fp);
			if (r->N_site[i][0])
			{
				fwrite(r->N_site[i]+1, sizeof(r->N_site[i][0]), r->N_site[i][0], fp);
			}
		}
		else
		{
			fwrite(&zero, sizeof(zero), 1, fp);
		}
		
		
		
	}

	fwrite(r->read_length, sizeof(uint64_t), r->total_reads, fp);
	for (i = 0; i < r->total_reads; i++)
	{
		fwrite(r->read_sperate[i], sizeof(uint8_t), r->read_length[i]/4+1, fp);
	}
	
	fwrite(r->name, sizeof(char), r->total_name_length, fp);
	fwrite(r->name_index, sizeof(uint64_t), r->name_index_size, fp);
    free(index_name);    
	fflush(fp);
    fclose(fp);
    fprintf(stderr, "Reads has been written.\n");
}



int load_All_reads(All_reads* r, char* read_file_name)
{
    fprintf(stderr, "Loading reads from disk... \n");
    char* index_name = (char*)malloc(strlen(read_file_name)+15);
    sprintf(index_name, "%s.bin", read_file_name);
    FILE* fp = fopen(index_name, "r");
	if (!fp)
    {
        return 0;
    }
	int local_adapterLen;
	int f_flag;
    f_flag = fread(&local_adapterLen, sizeof(local_adapterLen), 1, fp);
    if(local_adapterLen != asm_opt.adapterLen)
    {
        fprintf(stderr, "the adapterLen of index is: %d, but the adapterLen set by user is: %d\n", 
        local_adapterLen, asm_opt.adapterLen);
		exit(1);
    }
    f_flag += fread(&r->index_size, sizeof(r->index_size), 1, fp);
	f_flag += fread(&r->name_index_size, sizeof(r->name_index_size), 1, fp);
	f_flag += fread(&r->total_reads, sizeof(r->total_reads), 1, fp);
	f_flag += fread(&r->total_reads_bases, sizeof(r->total_reads_bases), 1, fp);
	f_flag += fread(&r->total_name_length, sizeof(r->total_name_length), 1, fp);

	uint64_t i = 0;
	uint64_t zero = 0;
	r->N_site = (uint64_t**)malloc(sizeof(uint64_t*)*r->total_reads);
	for (i = 0; i < r->total_reads; i++)
	{

		f_flag += fread(&zero, sizeof(zero), 1, fp);

		if (zero)
		{

			r->N_site[i] = (uint64_t*)malloc(sizeof(uint64_t)*(zero + 1));
			r->N_site[i][0] = zero;
			if (r->N_site[i][0])
			{
				f_flag += fread(r->N_site[i]+1, sizeof(r->N_site[i][0]), r->N_site[i][0], fp);
			}
		}
		else
		{
			r->N_site[i] = NULL;
		}

	}

	r->read_length = (uint64_t*)malloc(sizeof(uint64_t)*r->total_reads);
	f_flag += fread(r->read_length, sizeof(uint64_t), r->total_reads, fp);

	r->read_size = (uint64_t*)malloc(sizeof(uint64_t)*r->total_reads);
	memcpy (r->read_size, r->read_length, sizeof(uint64_t)*r->total_reads);

	r->read_sperate = (uint8_t**)malloc(sizeof(uint8_t*)*r->total_reads);
	for (i = 0; i < r->total_reads; i++)
    {
		r->read_sperate[i] = (uint8_t*)malloc(sizeof(uint8_t)*(r->read_length[i]/4+1));
		f_flag += fread(r->read_sperate[i], sizeof(uint8_t), r->read_length[i]/4+1, fp);
	}


	r->name = (char*)malloc(sizeof(char)*r->total_name_length);
	f_flag += fread(r->name, sizeof(char), r->total_name_length, fp);

	r->name_index = (uint64_t*)malloc(sizeof(uint64_t)*r->name_index_size);
	f_flag += fread(r->name_index, sizeof(uint64_t), r->name_index_size, fp);


	r->cigars = (Compressed_Cigar_record*)malloc(sizeof(Compressed_Cigar_record)*r->total_reads);
	r->second_round_cigar = (Compressed_Cigar_record*)malloc(sizeof(Compressed_Cigar_record)*r->total_reads);
	r->paf = (ma_hit_t_alloc*)malloc(sizeof(ma_hit_t_alloc)*r->total_reads);
	r->reverse_paf = (ma_hit_t_alloc*)malloc(sizeof(ma_hit_t_alloc)*r->total_reads);
	for (i = 0; i < r->total_reads; i++)
	{
		r->second_round_cigar[i].size = r->cigars[i].size = 0;
		r->second_round_cigar[i].length = r->cigars[i].length = 0;
		r->second_round_cigar[i].record = r->cigars[i].record = NULL;

		r->second_round_cigar[i].lost_base_size = r->cigars[i].lost_base_size = 0;
		r->second_round_cigar[i].lost_base_length = r->cigars[i].lost_base_length = 0;
		r->second_round_cigar[i].lost_base = r->cigars[i].lost_base = NULL;
		init_ma_hit_t_alloc(&(r->paf[i]));
		init_ma_hit_t_alloc(&(r->reverse_paf[i]));
	}

    free(index_name);    
    fclose(fp);
    fprintf(stderr, "Reads has been loaded.\n");

	return 1;
}



inline void insert_read(All_reads* r, kstring_t* read, kstring_t* name)
{
	r->total_reads++;
	r->total_reads_bases = r->total_reads_bases + read->l;
	r->total_name_length = r->total_name_length + name->l;

	///must +1
	if (r->index_size < r->total_reads + 2)
	{
		r->index_size = r->index_size * 2 + 2;
		r->read_length = (uint64_t*)realloc(r->read_length,sizeof(uint64_t)*(r->index_size));
		r->name_index_size = r->name_index_size * 2 + 2;
		r->name_index = (uint64_t*)realloc(r->name_index,sizeof(uint64_t)*(r->name_index_size));
	}

	r->read_length[r->total_reads - 1] = read->l;
	r->name_index[r->total_reads] = r->name_index[r->total_reads-1] + name->l;
}

void malloc_All_reads(All_reads* r)
{

	r->read_size = (uint64_t*)malloc(sizeof(uint64_t)*r->total_reads);
	memcpy (r->read_size, r->read_length, sizeof(uint64_t)*r->total_reads);

	r->read_sperate = (uint8_t**)malloc(sizeof(uint8_t*)*r->total_reads);
	long long i = 0;
	for (i = 0; i < (long long)r->total_reads; i++)
	{
		r->read_sperate[i] = (uint8_t*)malloc(sizeof(uint8_t)*(r->read_length[i]/4+1));
	}

	r->cigars = (Compressed_Cigar_record*)malloc(sizeof(Compressed_Cigar_record)*r->total_reads);
	r->second_round_cigar = (Compressed_Cigar_record*)malloc(sizeof(Compressed_Cigar_record)*r->total_reads);
	r->paf = (ma_hit_t_alloc*)malloc(sizeof(ma_hit_t_alloc)*r->total_reads);
	r->reverse_paf = (ma_hit_t_alloc*)malloc(sizeof(ma_hit_t_alloc)*r->total_reads);
	for (i = 0; i < (long long)r->total_reads; i++)
	{
		r->second_round_cigar[i].size = r->cigars[i].size = 0;
		r->second_round_cigar[i].length = r->cigars[i].length = 0;
		r->second_round_cigar[i].record = r->cigars[i].record = NULL;

		r->second_round_cigar[i].lost_base_size = r->cigars[i].lost_base_size = 0;
		r->second_round_cigar[i].lost_base_length = r->cigars[i].lost_base_length = 0;
		r->second_round_cigar[i].lost_base = r->cigars[i].lost_base = NULL;
		init_ma_hit_t_alloc(&(r->paf[i]));
		init_ma_hit_t_alloc(&(r->reverse_paf[i]));
	}

	r->name = (char*)malloc(sizeof(char)*r->total_name_length);
	r->N_site = (uint64_t**)calloc(r->total_reads, sizeof(uint64_t*));

}

void destory_UC_Read(UC_Read* r)
{
	free(r->seq);
}

void init_aux_table()
{
	if (bit_t_seq_table[0][0] == 0)
	{
		uint64_t i = 0;

		for (i = 0; i < 256; i++)
		{
			bit_t_seq_table[i][0] = s_H[((i >> 6)&(uint64_t)3)];
			bit_t_seq_table[i][1] = s_H[((i >> 4)&(uint64_t)3)];
			bit_t_seq_table[i][2] = s_H[((i >> 2)&(uint64_t)3)];
			bit_t_seq_table[i][3] = s_H[(i&(uint64_t)3)];

			bit_t_seq_table_rc[i][0] = RC_CHAR(bit_t_seq_table[i][3]);
			bit_t_seq_table_rc[i][1] = RC_CHAR(bit_t_seq_table[i][2]);
			bit_t_seq_table_rc[i][2] = RC_CHAR(bit_t_seq_table[i][1]);
			bit_t_seq_table_rc[i][3] = RC_CHAR(bit_t_seq_table[i][0]);
		}
		
	}
}

void init_UC_Read(UC_Read* r)
{
	r->length = 0;
	r->size = 0;
	r->seq = NULL;
	if (bit_t_seq_table[0][0] == 0)
	{
		uint64_t i = 0;

		for (i = 0; i < 256; i++)
		{
			bit_t_seq_table[i][0] = s_H[((i >> 6)&(uint64_t)3)];
			bit_t_seq_table[i][1] = s_H[((i >> 4)&(uint64_t)3)];
			bit_t_seq_table[i][2] = s_H[((i >> 2)&(uint64_t)3)];
			bit_t_seq_table[i][3] = s_H[(i&(uint64_t)3)];

			bit_t_seq_table_rc[i][0] = RC_CHAR(bit_t_seq_table[i][3]);
			bit_t_seq_table_rc[i][1] = RC_CHAR(bit_t_seq_table[i][2]);
			bit_t_seq_table_rc[i][2] = RC_CHAR(bit_t_seq_table[i][1]);
			bit_t_seq_table_rc[i][3] = RC_CHAR(bit_t_seq_table[i][0]);
		}
		
	}
	
}


void recover_UC_Read_sub_region_begin_end
(char* r, long long start_pos, long long length, uint8_t strand, All_reads* R_INF, long long ID, int extra_begin, int extra_end)
{



	long long readLen = Get_READ_LENGTH((*R_INF), ID);
	uint8_t* src = Get_READ((*R_INF), ID);

	long long i;
	long long copyLen;
	long long end_pos = start_pos + length - 1;


	

	if (strand == 0)
	{
		
		i = start_pos;
		copyLen = 0;



		long long initLen = start_pos % 4;

		if (initLen != 0)
		{
			memcpy(r, bit_t_seq_table[src[i>>2]] + initLen, 4 - initLen);
			copyLen = copyLen + 4 - initLen;
			i = i + copyLen;
		}
		
		
		while (copyLen < length)
		{
			memcpy(r+copyLen, bit_t_seq_table[src[i>>2]], 4);
			copyLen = copyLen + 4;
			i = i + 4;
		}

		
		if (R_INF->N_site[ID])
		{
			for (i = 1; i <= (long long)R_INF->N_site[ID][0]; i++)
			{
				if ((long long)R_INF->N_site[ID][i] >= start_pos && (long long)R_INF->N_site[ID][i] <= end_pos)
				{
					r[R_INF->N_site[ID][i] - start_pos] = 'N';
				}
				else if((long long)R_INF->N_site[ID][i] > end_pos)
				{
					break;
				}
			}
		}
		
		
	}
	else
	{
		
		start_pos = readLen - start_pos - 1;
		end_pos = readLen - end_pos - 1;



		///start_pos > end_pos
		i = start_pos;
		copyLen = 0;
		long long initLen = (start_pos + 1) % 4;

		if (initLen != 0)
		{
			memcpy(r, bit_t_seq_table_rc[src[i>>2]] + 4 - initLen, initLen);
			copyLen = copyLen + initLen;
			i = i - initLen;
		}

		while (copyLen < length)
		{
			memcpy(r+copyLen, bit_t_seq_table_rc[src[i>>2]], 4);
			copyLen = copyLen + 4;
			i = i - 4;
		}

		if (R_INF->N_site[ID])
		{
			long long offset = readLen - start_pos - 1;

			for (i = 1; i <= (long long)R_INF->N_site[ID][0]; i++)
			{

				if ((long long)R_INF->N_site[ID][i] >= end_pos && (long long)R_INF->N_site[ID][i] <= start_pos)
				{
					r[readLen - R_INF->N_site[ID][i] - 1 - offset] = 'N';
				}
				else if((long long)R_INF->N_site[ID][i] > start_pos)
				{
					break;
				}
			}
		}
		

		
	}
		
}




void recover_UC_Read_sub_region(char* r, long long start_pos, long long length, uint8_t strand, All_reads* R_INF, long long ID)
{



	long long readLen = Get_READ_LENGTH((*R_INF), ID);
	uint8_t* src = Get_READ((*R_INF), ID);

	long long i;
	long long copyLen;
	long long end_pos = start_pos + length - 1;

	if (strand == 0)
	{
		
		i = start_pos;
		copyLen = 0;

		long long initLen = start_pos % 4;

		if (initLen != 0)
		{
			memcpy(r, bit_t_seq_table[src[i>>2]] + initLen, 4 - initLen);
			copyLen = copyLen + 4 - initLen;
			i = i + copyLen;
		}
		
		
		while (copyLen < length)
		{
			memcpy(r+copyLen, bit_t_seq_table[src[i>>2]], 4);
			copyLen = copyLen + 4;
			i = i + 4;
		}

		
		if (R_INF->N_site[ID])
		{
			for (i = 1; i <= (long long)R_INF->N_site[ID][0]; i++)
			{
				if ((long long)R_INF->N_site[ID][i] >= start_pos && (long long)R_INF->N_site[ID][i] <= end_pos)
				{
					r[R_INF->N_site[ID][i] - start_pos] = 'N';
				}
				else if((long long)R_INF->N_site[ID][i] > end_pos)
				{
					break;
				}
			}
		}
		
		
	}
	else
	{
		
		start_pos = readLen - start_pos - 1;
		end_pos = readLen - end_pos - 1;



		///start_pos > end_pos
		i = start_pos;
		copyLen = 0;
		long long initLen = (start_pos + 1) % 4;

		if (initLen != 0)
		{
			memcpy(r, bit_t_seq_table_rc[src[i>>2]] + 4 - initLen, initLen);
			copyLen = copyLen + initLen;
			i = i - initLen;
		}

		while (copyLen < length)
		{
			memcpy(r+copyLen, bit_t_seq_table_rc[src[i>>2]], 4);
			copyLen = copyLen + 4;
			i = i - 4;
		}

		if (R_INF->N_site[ID])
		{
			long long offset = readLen - start_pos - 1;

			for (i = 1; i <= (long long)R_INF->N_site[ID][0]; i++)
			{

				if ((long long)R_INF->N_site[ID][i] >= end_pos && (long long)R_INF->N_site[ID][i] <= start_pos)
				{
					r[readLen - R_INF->N_site[ID][i] - 1 - offset] = 'N';
				}
				else if((long long)R_INF->N_site[ID][i] > start_pos)
				{
					break;
				}
			}
		}
		

		
	}
		
}


void recover_UC_Read(UC_Read* r, All_reads* R_INF, uint64_t ID)
{
	r->length = Get_READ_LENGTH((*R_INF), ID);
	uint8_t* src = Get_READ((*R_INF), ID);

	if (r->length + 4 > r->size)
	{
		r->size = r->length + 4;
		r->seq = (char*)realloc(r->seq,sizeof(char)*(r->size));
	}

	uint64_t i = 0;

	while ((long long)i < r->length)
	{
		memcpy(r->seq+i, bit_t_seq_table[src[i>>2]], 4);
		i = i + 4;
	}


	if (R_INF->N_site[ID])
	{
		for (i = 1; i <= R_INF->N_site[ID][0]; i++)
		{
			r->seq[R_INF->N_site[ID][i]] = 'N';
		}
	}

	r->RID = ID;
		
}

void recover_UC_Read_RC(UC_Read* r, All_reads* R_INF, uint64_t ID)
{
	r->length = Get_READ_LENGTH((*R_INF), ID);
	uint8_t* src = Get_READ((*R_INF), ID);

	if (r->length + 4 > r->size)
	{
		r->size = r->length + 4;
		r->seq = (char*)realloc(r->seq,sizeof(char)*(r->size));
	}

	long long last_chr = r->length % 4;
	long long i = r->length / 4 - 1 + (last_chr != 0);
	long long index = 0;

	if(last_chr!=0)
	{
		memcpy(r->seq + index, bit_t_seq_table_rc[src[i]] + 4 - last_chr, last_chr);
		index = last_chr;
		i--;
	}

	while (i >= 0)
	{
		memcpy(r->seq + index, bit_t_seq_table_rc[src[i]], 4);
		i--;
		index = index + 4;
	}


	if (R_INF->N_site[ID])
	{
		for (i = 1; i <= (long long)R_INF->N_site[ID][0]; i++)
		{
			r->seq[r->length - R_INF->N_site[ID][i] - 1] = 'N';
		}
	}
		
}



#define COMPRESS_BASE {c = seq_nt6_table[(uint8_t)src[i]];\
		if (c >= 4)\
		{\
			c = 0;\
			(*N_site_lis)[N_site_i] = i;\
			N_site_i++;\
		}\
		i++;}\

void compress_base(uint8_t* dest, char* src, uint64_t src_l, uint64_t** N_site_lis, uint64_t N_site_occ)
{

	///N_site_lis saves the pos of all Ns in this read
	///N_site_lis[0] is the number of Ns
	if (N_site_occ)
	{
		(*N_site_lis) = (uint64_t*)malloc(sizeof(uint64_t)*(N_site_occ + 1));
		(*N_site_lis)[0] = N_site_occ;
	}
	else
	{
		(*N_site_lis) = NULL;
	}

	uint64_t i = 0;
	uint64_t N_site_i = 1;
	uint64_t dest_i = 0;
	uint8_t tmp = 0;
	uint8_t c = 0;


	while (i + 4 <= src_l)
	{

		tmp = 0;

		COMPRESS_BASE;
		tmp = tmp | (c<<6);

		COMPRESS_BASE;
		tmp = tmp | (c<<4);

		COMPRESS_BASE;
		tmp = tmp | (c<<2);

		COMPRESS_BASE;
		tmp = tmp | c;

		dest[dest_i] = tmp;

		dest_i++;
	}

	//at most 3 bases here
	uint64_t shift = 6;
	if (i < src_l)
	{
		tmp = 0;

		while (i < src_l)
		{
			COMPRESS_BASE;
			tmp = tmp | (c << shift);
			shift = shift -2;
		}
		
		dest[dest_i] = tmp;
		dest_i++;
	}

}




void open_file(gz_files* nfps, char* name)
{
	nfps->fp = gzopen(name, "r");
	if(nfps->fp == 0)
	{
		fprintf(stderr, "[ERROR] Cannot find the input file: %s\n", name);
		exit(0);
	}
	nfps->seq = kseq_init(nfps->fp);
}


void close_file(gz_files* nfps)
{
	kseq_destroy(nfps->seq);
  	gzclose(nfps->fp);
}

void init_gz_files(hifiasm_opt_t* asm_opt)
{
	fps.idx = 0;
	fps.num_reads = asm_opt->num_reads;
	fps.reads = asm_opt->read_file_names;
	fps.seq = NULL;
	fps.fp = NULL;
	if(fps.num_reads > 0)
	{
		open_file(&fps, fps.reads[fps.idx]);
		fps.idx++;
	}
}

void destory_gz_files()
{
	close_file(&fps);
}

int read_item()
{
	int l = kseq_read(fps.seq);
	if(l >= 0 || (l < 0 && fps.idx >= fps.num_reads))
	{
		return l;
	}
	///l < 0 && fps.idx < fps.num_reads
	close_file(&fps);
	open_file(&fps, fps.reads[fps.idx]);
	fps.idx++;
	return read_item();
}


inline void exchage_kstring_t(kstring_t* a, kstring_t* b)
{
	kstring_t tmp;
	tmp = *a;
	*a = *b;
	*b = tmp;
}

int get_read(kseq_t *s, int adapterLen)
{
	int l;
	
	///if ((l = kseq_read(seq)) >= 0)
	if ((l = read_item()) >= 0)
	{
		
		exchage_kstring_t(&(fps.seq->comment), &s->comment);
		exchage_kstring_t(&(fps.seq->name), &s->name);
		exchage_kstring_t(&(fps.seq->qual), &s->qual);
		exchage_kstring_t(&(fps.seq->seq), &s->seq);

		if(adapterLen > 0)
		{
			if((int)s->seq.l <= adapterLen*2)
			{
				s->seq.l = 0;
			}
			else
			{
				long long i;
				for (i = 0; i < ((int)s->seq.l - adapterLen*2); i++)
				{
					s->seq.s[i] = s->seq.s[i + adapterLen];
				}
				s->seq.l -= adapterLen*2;
			} 
			
		}
		
		return 1;
	}
	else
	{
		return 0;
	}
	
	
}

void init_R_buffer_block(R_buffer_block* curr_sub_block)
{
	curr_sub_block->read = (kseq_t*)calloc(RDB.block_inner_size, sizeof(kseq_t));	
	curr_sub_block->num = 0;
}

void clear_R_buffer()
{
	RDB.all_read_end = 0;
	RDB.num = 0;
}
void init_R_buffer(int thread_num)
{
	RDB.all_read_end = 0;
	RDB.num = 0;
	RDB.block_inner_size = READ_BLOCK_SIZE;
	RDB.size = thread_num*READ_BLOCK_NUM_PRE_THR;

	RDB.sub_block = (R_buffer_block*)malloc(sizeof(R_buffer_block)*RDB.size);

	int i = 0;

	for (i = 0; i < RDB.size; i++)
	{
		init_R_buffer_block(&RDB.sub_block[i]);
	}
	
}


void destory_R_buffer_block(R_buffer_block* curr_sub_block)
{
	kseq_destroy(curr_sub_block->read);
}


void destory_R_buffer()
{
	int i = 0;

	for (i = 0; i < RDB.size; i++)
	{
		destory_R_buffer_block(&RDB.sub_block[i]);
	}

	free(RDB.sub_block);
	
}


inline void load_read_block(R_buffer_block* read_batch, int batch_read_size,
	int* return_file_flag, int is_insert, int adapterLen)
{
	int inner_i = 0;
	int file_flag = 1;




	while (inner_i<batch_read_size)
	{

		file_flag = get_read(&read_batch->read[inner_i], adapterLen);

		if (file_flag == 1)
		{
			read_batch->read[inner_i].ID = total_reads;
			total_reads++;


			if (is_insert)
			{
				insert_read(&R_INF, &read_batch->read[inner_i].seq, 
				&read_batch->read[inner_i].name);
			}

			inner_i++;
		}
		else if (file_flag == 0)
		{
			break;
		}
	}

	if (inner_i || file_flag)
	{
		file_flag = 1;
	}

	*return_file_flag = file_flag;
	read_batch->num = inner_i;

}


inline void push_R_block(R_buffer_block* tmp_sub_block)
{
	

	///only exchange pointers
	kseq_t *k1;
	k1 = RDB.sub_block[RDB.num].read;

	RDB.sub_block[RDB.num].read = tmp_sub_block->read;

	tmp_sub_block->read = k1;

	RDB.sub_block[RDB.num].num = tmp_sub_block->num;
	tmp_sub_block->num = 0;

	RDB.num++;
}


inline void pop_R_block(R_buffer_block* curr_sub_block)
{
	RDB.num--;
	
	///only exchange pointers
	kseq_t *k1;
	k1 = RDB.sub_block[RDB.num].read;

	RDB.sub_block[RDB.num].read = curr_sub_block->read;

	curr_sub_block->read = k1;

	curr_sub_block->num = RDB.sub_block[RDB.num].num;
	RDB.sub_block[RDB.num].num = 0;



}



void* input_reads_muti_threads(void* arg)
{
	int is_insert = *((int*)arg);


	total_reads = 0;


	int file_flag = 1;

	R_buffer_block tmp_buf;

	init_R_buffer_block(&tmp_buf);



	while (1)
	{
		load_read_block(&tmp_buf, RDB.block_inner_size, &file_flag, is_insert, asm_opt.adapterLen);

		if (file_flag == 0)
		{
			break;
		}


		pthread_mutex_lock(&i_readinputMutex);
		while (IS_FULL(RDB))
		{

			pthread_cond_signal(&i_readinputstallCond);
			pthread_cond_wait(&i_readinputflushCond, &i_readinputMutex);
		}


		push_R_block(&tmp_buf);

		pthread_cond_signal(&i_readinputstallCond);
		pthread_mutex_unlock(&i_readinputMutex);
	}


	pthread_mutex_lock(&i_readinputMutex);
	RDB.all_read_end = 1;
	pthread_cond_signal(&i_readinputstallCond);  //important
	pthread_mutex_unlock(&i_readinputMutex);

	destory_R_buffer_block(&tmp_buf);

	fprintf(stderr, "Reads #: %lu\n", (unsigned long)total_reads);
	fprintf(stderr, "Bases #: %lu\n", (unsigned long)R_INF.total_reads_bases);
	

	return NULL;
}



int get_reads_mul_thread(R_buffer_block* curr_sub_block)
{


	pthread_mutex_lock(&i_readinputMutex);


	while (IS_EMPTY(RDB) && RDB.all_read_end == 0)
	{

		pthread_cond_signal(&i_readinputflushCond);
		pthread_cond_wait(&i_readinputstallCond, &i_readinputMutex);
	}


	if (!IS_EMPTY(RDB))
	{
		pop_R_block(curr_sub_block);
		pthread_cond_signal(&i_readinputflushCond);
		pthread_mutex_unlock(&i_readinputMutex);


		return 1;
	}
	else
	{
		curr_sub_block->num = 0;

		pthread_cond_signal(&i_readinputstallCond);   //important

		pthread_mutex_unlock(&i_readinputMutex);

		return 0;
	}
	

}








void reverse_complement(char* pattern, uint64_t length)
{
	uint64_t i = 0;
	uint64_t end = length / 2;
	char k;
	uint64_t index;

	for (i = 0; i < end; i++)
	{
		index = length - i - 1;
		k = pattern[index];
		pattern[index] = RC_CHAR(pattern[i]);
		pattern[i] = RC_CHAR(k);
	}

	if(length&(uint64_t)1)
	{
		pattern[end] = RC_CHAR(pattern[end]);
	}

}


