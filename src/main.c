#include "interpreter/spyre.h"
#include "interpreter/const.h"

int main(int argc, char** argv) {

	const uint8 code[] = {
	
		0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 

		0x02, 0x00	
		
	};

	spy_state* S = spy_newstate();
	spy_run(S, code, sizeof(code));

	return 0;

}
