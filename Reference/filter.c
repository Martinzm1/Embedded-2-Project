#include <stdio.h>
#include <omp.h>

#define SAMPLES 6001
#define B_COEF 101

// Clint Wetzel

void filterSignal(double *d_samples, double *d_coef, double *d_fsignal);
double maxSignal(double *d_signal, int length);
double avgSignal(double *d_signal, double average_offset, int length);

int main(void){
	int i;
	double max, average;
	double d_samples[SAMPLES];
	double d_coef[B_COEF];
	double d_fsignal[SAMPLES+B_COEF-1] = {0};
		
	FILE *fp = fopen("./100Hz.txt", "r");
	for(i = 0; i < SAMPLES-1; i++){
		fscanf(fp, "%lf\n", &d_samples[i]);
	}
	fclose(fp);

	fp = fopen("./b.txt", "r");
	for(i = 0; i < B_COEF-1; i++){
		fscanf(fp, "%lf\n", &d_coef[i]);
	}
	fclose(fp);

	filterSignal(d_samples, d_coef, d_fsignal);
	max = maxSignal(d_fsignal, SAMPLES+B_COEF-1);
	average = avgSignal(d_fsignal, 100.0, SAMPLES+B_COEF-1);
	
	printf("max: %lf\naverage: %lf\n", max, average);
}

void filterSignal(double *d_samples, double *d_coef, double *d_fsignal){
	int i, j;

	# pragma omp parallel for private(j)
	for(i = 0; i < B_COEF-1; i++){
		for(j = 0; j < SAMPLES-1; j++){
			d_fsignal[i+j] = d_fsignal[i+j] + (d_samples[j] * d_coef[i]);
		}
	}
}

double maxSignal(double *d_signal, int length){
	int i;
	double max = d_signal[0];

	for(i = 0; i < length; i++){
		if(d_signal[i] > max){
			max = d_signal[i];
		}
	}

	return max;
}

double avgSignal(double *d_signal, double average_offset, int length){
	int i, avg;
	double sum = 0.0;

	# pragma omp parallel for reduction(+:sum)
	for(i = average_offset; i < length; i++){
		sum = sum + d_signal[i];
	}
	avg = sum/((double)length - average_offset);

	return avg;
}
