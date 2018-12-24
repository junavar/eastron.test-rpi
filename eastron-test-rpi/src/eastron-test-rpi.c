#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <stdint.h>
#include <errno.h>

#include <fcntl.h>
#include <termios.h>

union {
	char c[4];
	float f;
} u;

int abre_puerto_serie(char * nombre_dispositivo) {

	struct termios options;
	int fd = open(nombre_dispositivo, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd == -1) {
		perror("Error abriendo fichero");
		return -1;
	}
	fprintf(stderr, "Ok. Abierto con handle %d\n", fd);

	//////////////////// Set up Serial Configuration ////////////
	options.c_iflag = 0; //input mode flags
	options.c_oflag = 0; // output mode flags
	options.c_lflag = 0; //local mode flags
	options.c_cflag = 0; //control mode flags
	if (tcgetattr(fd, &options) < 0) {
		perror("Getting configuration");
		return -1;
	}
	cfmakeraw(&options); /* pone los ajuste de modo raw en la variable options*/
	//serial.c_cflag = B9600 | CS8 | PARENB | CREAD & ~PARODD;
	options.c_cflag = B9600 | CS8;

	//options.c_cc[VMIN] = 1; // espera al menos un byte en read(). Puede bloquear read() si VTIME=0
	options.c_cc[VMIN] = 0; // read() retorna aunque no haya bytes
	//options.c_cc[VTIME] = 0; // no temporacion. La función read() puede quedar bloqueada indefinidamente hasta que lleguen bytes
	options.c_cc[VTIME] = 5; // Temporacion intercaracter de 0,5s si VMIN>0; temporizacion total del read() si VMIN=0

	fcntl(fd, F_SETFL, 0);
	// Aplica la configuration definida en la variable options
	tcsetattr(fd, TCSANOW, &options);
	tcflush(fd, TCIOFLUSH);
	//////////////////////////////////////////////////////////////
	return fd;

}

int main(int argc, char *argv[]) {

	char *nombre_dispositivo_default = "/dev/ttyUSB0";
	char *nombre_dispositivo;
	int fd;

	unsigned char inBuff[16];
	unsigned char *buffer;
	unsigned char cVoltage[] =
			{ 0x01, 0x04, 0x00, 0x00, 0x00, 0x02, 0x71, 0xCB };
	unsigned char cCurrent[] =
			{ 0x01, 0x04, 0x00, 0x06, 0x00, 0x02, 0x91, 0xCA };
	unsigned char cPower[] = { 0x01, 0x04, 0x00, 0x0C, 0x00, 0x02, 0xB1, 0xC8 };
	unsigned char cAAPower[] =
			{ 0x01, 0x04, 0x00, 0x12, 0x00, 0x02, 0xD1, 0xCE };
	unsigned char cRAPower[] =
			{ 0x01, 0x04, 0x00, 0x18, 0x00, 0x02, 0xF1, 0xCC };
	unsigned char cPFactor[] =
			{ 0x01, 0x04, 0x00, 0x1E, 0x00, 0x02, 0x11, 0xCD };
	unsigned char cFrequency[] = { 0x01, 0x04, 0x00, 0x46, 0x00, 0x02, 0x90,
			0x1E };
	unsigned char cIAEnergy[] =
			{ 0x01, 0x04, 0x00, 0x48, 0x00, 0x02, 0xF1, 0xDD };
	unsigned char cEAEnergy[] =
			{ 0x01, 0x04, 0x00, 0x4A, 0x00, 0x02, 0x50, 0x1D };
	unsigned char cTAEnergy[] =
			{ 0x01, 0x04, 0x01, 0x56, 0x00, 0x02, 0x90, 0x27 };

	if (argc == 2) {
		nombre_dispositivo = argv[1];
	} else {
		nombre_dispositivo = nombre_dispositivo_default;
	}
	fprintf(stderr, "Abriendo el device %s ...", nombre_dispositivo);
	fd = abre_puerto_serie(nombre_dispositivo);

	int rcount;
	int wcount;
	int i = 0;
	int j = 0;

	///////////////////////////////         Power               ///////////////////////////////

	wcount = write(fd, cPower, sizeof(cPower));
	if (wcount < 0) {
		perror("Write");
		return -1;
	}
	fsync(fd);

	fprintf(stderr, "Sent: ");
	for (j = 0; j < wcount; j++) {
		fprintf(stderr, "%02x ", cPower[j]);
	}

	// este retardo es importante para que llegue los 9 caracters de la respuesta en un único read
	// El chip de Prolific requiere 100ms para recibir los 9 bytes de una trama en cable corto 1,5m a 9600bps. (1 fallo de 40).
	// Por seguridad se pone más retardo
	// El chip CH-340 es  más rápido 10us funciona para la misma longitud
	// No se ha comprobado la incidencia con la longitud del  cable
	usleep(150000);

	buffer = &inBuff[0];
	memset(&inBuff[0], 0, sizeof(inBuff));
	rcount = read(fd, buffer, sizeof(inBuff));

	if (rcount < 0) {
		perror("Read");
		return -1;
	}

	fprintf(stderr, "\n");
	fprintf(stderr, "Received %d bytes: ", rcount);
	for (i = 0; i < rcount; i++) {
		fprintf(stderr, "%02x ", inBuff[i]);
	}
	fprintf(stderr, "\n");

	if (rcount == 9) {
		u.c[3] = inBuff[3];
		u.c[2] = inBuff[4];
		u.c[1] = inBuff[5];
		u.c[0] = inBuff[6];
		printf("Potencia: %.0f W\n", u.f);
	}

	close(fd);

	return 0;
}
