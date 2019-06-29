#include "Chip8.h"

#include "Macros.h"

#include <spdlog/spdlog.h>

#include <array>
#include <cassert>
#include <bitset>
#include <iostream>

namespace c8
{

	class BitView
	{
	public:


		BitView(void * ptr, size_t length) :
			m_Ptr((uint8_t*)ptr),
			m_Count(length * 8)
		{
		}

		size_t Count() const
		{
			return m_Count;
		}

		bool Get(size_t bit) const
		{
			return m_Ptr[bit / 8] & (0x80 >> (bit % 8));
		}

		void Set(size_t bit, bool value)
		{
			uint8_t mod = bit % 8;
			uint8_t mask = 0x80 >> mod;
			value ? m_Ptr[bit / 8] |= mask : m_Ptr[bit / 8] &= ~mask;
		}


	private:
		size_t m_Count;
		uint8_t *m_Ptr;
	};

	static const std::string SplashScreenData =
		"................................................................"
		"................................................................"
		"................................................................"
		"................................................................"
		"................................................................"
		"................................................................"
		"................................................................"
		"................................................................"
		"................................................................"
		"................................................................"
		"................................................................"
		"............XXXXXXX..XX...XX..XX..XXXXXXX....XXXXXXX............"
		"............XX.......XX...XX..XX..XX...XX....XX...XX............"
		"............XX.......XX...XX..XX..XX...XX....XX...XX............"
		"............XX.......XX...XX..XX..XX...XX....XX...XX............"
		"............XX.......XXXXXXX..XX..XXXXXXX....XXXXXXX............"
		"............XX.......XX...XX..XX..XX.........XX...XX............"
		"............XX.......XX...XX..XX..XX.........XX...XX............"
		"............XX.......XX...XX..XX..XX.........XX...XX............"
		"............XXXXXXX..XX...XX..XX..XX.........XXXXXXX............"
		"................................................................"
		"................................................................"
		"................................................................"
		"................................................................"
		"................................................................"
		"................................................................"
		"................................................................"
		"................................................................"
		"................................................................"
		"................................................................"
		"................................................................"
		"................................................................"
		;

	struct Chip8::Impl
	{
		/*	--- Memory ---
			CHIP-8 was most commonly implemented on 4K systems, such as the Cosmac VIP and the Telmac 1800. These machines had 4096 (0x1000) 
			memory locations, all of which are 8 bits (a byte) which is where the term CHIP-8 originated. However, the CHIP-8 interpreter itself 
			occupies the first 512 bytes of the memory space on these machines. For this reason, most programs written for the original system begin 
			at memory location 512 (0x200) and do not access any of the memory below the location 512 (0x200). The uppermost 256 bytes (0xF00-0xFFF) 
			are reserved for display refresh, and the 96 bytes below that (0xEA0-0xEFF) were reserved for the call stack, internal use, and other variables.
		*/

		static constexpr uint32_t TotalMemory = 0x1000;
		static constexpr uint32_t ProgramStartAddress = 0x200;
		static constexpr uint32_t DisplayAddress = 0xf00;
		static constexpr uint32_t StackAddress = 0xea0;
		static constexpr uint32_t ProgramMemory = TotalMemory - ProgramStartAddress - (TotalMemory - StackAddress);

		std::array<uint8_t, TotalMemory> Memory;

		


		/*	--- Registers ---
			CHIP-8 has 16 8-bit data registers named V0 to VF. The VF register doubles as a flag for some instructions; thus, it should be avoided. 
			In an addition operation, VF is the carry flag, while in subtraction, it is the "no borrow" flag. In the draw instruction VF is set upon pixel collision.

			The address register, which is named I, is 16 bits wide and is used with several opcodes that involve memory operations.
		*/
		static constexpr uint8_t NumRegisters = 16;

		std::array<uint8_t, NumRegisters> V; // GP registers
		uint16_t PC;	// Program counter
		uint16_t I;		// Address register

		/*	--- The stack ---
			The stack is only used to store return addresses when subroutines are called.The original 1802 version allocated 48 bytes for up to
			24 levels of nesting; modern implementations normally have at least 16 levels.
		*/
		uint8_t SP;		// Stack pointer


		/*	--- Timers ---
			CHIP-8 has two timers. They both count down at 60 hertz, until they reach 0.

			Delay timer: This timer is intended to be used for timing the events of games. Its value can be set and read.
			Sound timer: This timer is used for sound effects. When its value is nonzero, a beeping sound is made.
		
		*/
		static constexpr float TimePeriod = 1 / 60.0f;

		uint8_t DelayTimer;
		uint8_t SoundTimer;
		float TimeCounter;
		

		/*	--- Input --- 
			Input is done with a hex keyboard that has 16 keys ranging 0 to F. The '8', '4', '6', and '2' keys are typically used for 
			directional input. Three opcodes are used to detect input. One skips an instruction if a specific key is pressed, while another does 
			the same if a specific key is not pressed. The third waits for a key press, and then stores it in one of the data registers.
		*/
		static constexpr uint8_t NumKeys = 16;
		std::array<bool, NumKeys> Keyboard;
		
		static constexpr std::array<uint8_t, NumKeys * 5> Characters = {
			0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
			0x20, 0x60, 0x20, 0x20, 0x70, // 1
			0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
			0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
			0x90, 0x90, 0xF0, 0x10, 0x10, // 4
			0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
			0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
			0xF0, 0x10, 0x20, 0x40, 0x40, // 7
			0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
			0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
			0xF0, 0x90, 0xF0, 0x90, 0x90, // A
			0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
			0xF0, 0x80, 0x80, 0x80, 0xF0, // C
			0xE0, 0x90, 0x90, 0x90, 0xE0, // D
			0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
			0xF0, 0x80, 0xF0, 0x80, 0x80, // F
		};
		



		/*	--- Graphics and sound ---
			Original CHIP-8 Display resolution is 64×32 pixels, and color is monochrome. Graphics are drawn to the screen solely by 
			drawing sprites, which are 8 pixels wide and may be from 1 to 15 pixels in height. Sprite pixels are XOR'd with 
			corresponding screen pixels. In other words, sprite pixels that are set flip the color of the corresponding screen pixel, 
			while unset sprite pixels do nothing. The carry flag (VF) is set to 1 if any screen pixels are flipped from set to unset 
			when a sprite is drawn and set to 0 otherwise. This is used for collision detection.
		
		*/
		static constexpr uint8_t ScreenWidth = 64;
		static constexpr uint8_t ScreenHeight = 32;
		BitView VideoMemory = { (void*)&Memory[DisplayAddress], 256 };

		/*
			CHIP-8 has 35 opcodes, which are all two bytes long and stored big-endian. The opcodes are listed below, in hexadecimal and with the following symbols:

			NNN: address
			NN: 8-bit constant
			N: 4-bit constant
			X and Y: 4-bit register identifier
			PC : Program Counter
			I : 16bit register (For memory address) (Similar to void pointer)
			VN: One of the 16 available variables. N may be 0 to F (hexadecimal)

			Opcode	Type	C Pseudo	Explanation
			0NNN	Call		Calls RCA 1802 program at address NNN. Not necessary for most ROMs.
			00E0	Display	disp_clear()	Clears the screen.
			00EE	Flow	return;	Returns from a subroutine.
			1NNN	Flow	goto NNN;	Jumps to address NNN.
			2NNN	Flow	*(0xNNN)()	Calls subroutine at NNN.
			3XNN	Cond	if(Vx==NN)	Skips the next instruction if VX equals NN. (Usually the next instruction is a jump to skip a code block)
			4XNN	Cond	if(Vx!=NN)	Skips the next instruction if VX doesn't equal NN. (Usually the next instruction is a jump to skip a code block)
			5XY0	Cond	if(Vx==Vy)	Skips the next instruction if VX equals VY. (Usually the next instruction is a jump to skip a code block)
			6XNN	Const	Vx = NN	Sets VX to NN.
			7XNN	Const	Vx += NN	Adds NN to VX. (Carry flag is not changed)
			8XY0	Assign	Vx=Vy	Sets VX to the value of VY.
			8XY1	BitOp	Vx=Vx|Vy	Sets VX to VX or VY. (Bitwise OR operation)
			8XY2	BitOp	Vx=Vx&Vy	Sets VX to VX and VY. (Bitwise AND operation)
			8XY3	BitOp	Vx=Vx^Vy	Sets VX to VX xor VY.
			8XY4	Math	Vx += Vy	Adds VY to VX. VF is set to 1 when there's a carry, and to 0 when there isn't.
			8XY5	Math	Vx -= Vy	VY is subtracted from VX. VF is set to 0 when there's a borrow, and 1 when there isn't.
			8XY6	BitOp	Vx>>=1	Stores the least significant bit of VX in VF and then shifts VX to the right by 1.[2]
			8XY7	Math	Vx=Vy-Vx	Sets VX to VY minus VX. VF is set to 0 when there's a borrow, and 1 when there isn't.
			8XYE	BitOp	Vx<<=1	Stores the most significant bit of VX in VF and then shifts VX to the left by 1.[3]
			9XY0	Cond	if(Vx!=Vy)	Skips the next instruction if VX doesn't equal VY. (Usually the next instruction is a jump to skip a code block)
			ANNN	MEM	I = NNN	Sets I to the address NNN.
			BNNN	Flow	PC=V0+NNN	Jumps to the address NNN plus V0.
			CXNN	Rand	Vx=rand()&NN	Sets VX to the result of a bitwise and operation on a random number (Typically: 0 to 255) and NN.
			DXYN	Disp	draw(Vx,Vy,N)	Draws a sprite at coordinate (VX, VY) that has a width of 8 pixels and a height of N pixels. Each row of 8 pixels is read as bit-coded starting from memory location I; I value doesn’t change after the execution of this instruction. As described above, VF is set to 1 if any screen pixels are flipped from set to unset when the sprite is drawn, and to 0 if that doesn’t happen
			EX9E	KeyOp	if(key()==Vx)	Skips the next instruction if the key stored in VX is pressed. (Usually the next instruction is a jump to skip a code block)
			EXA1	KeyOp	if(key()!=Vx)	Skips the next instruction if the key stored in VX isn't pressed. (Usually the next instruction is a jump to skip a code block)
			FX07	Timer	Vx = get_delay()	Sets VX to the value of the delay timer.
			FX0A	KeyOp	Vx = get_key()	A key press is awaited, and then stored in VX. (Blocking Operation. All instruction halted until next key event)
			FX15	Timer	delay_timer(Vx)	Sets the delay timer to VX.
			FX18	Sound	sound_timer(Vx)	Sets the sound timer to VX.
			FX1E	MEM	I +=Vx	Adds VX to I.[4]
			FX29	MEM	I=sprite_addr[Vx]	Sets I to the location of the sprite for the character in VX. Characters 0-F (in hexadecimal) are represented by a 4x5 font.
			FX33	BCD	set_BCD(Vx);
			*(I+0)=BCD(3);

			*(I+1)=BCD(2);

			*(I+2)=BCD(1);

			Stores the binary-coded decimal representation of VX, with the most significant of three digits at the address in I, the middle digit at I plus 1, and the least significant digit at I plus 2. (In other words, take the decimal representation of VX, place the hundreds digit in memory at location in I, the tens digit at location I+1, and the ones digit at location I+2.)
			FX55	MEM	reg_dump(Vx,&I)	Stores V0 to VX (including VX) in memory starting at address I. The offset from I is increased by 1 for each value written, but I itself is left unmodified.
			FX65	MEM	reg_load(Vx,&I)	Fills V0 to VX (including VX) with values from memory starting at address I. The offset from I is increased by 1 for each value written, but I itself is left unmodified.
		
		*/

		Impl()
		{
			Reset();

			for (uint16_t x = 0; x < ScreenWidth; x++)
			{
				for (uint16_t y = 0; y < ScreenHeight; y++)
				{
					VideoMemory.Set(y * ScreenWidth + x, SplashScreenData[y * ScreenWidth + x] == 'X');
				}
			}

		}

		void Reset()
		{
			memset(Memory.data(), 0, TotalMemory);
			memset(V.data(), 0, NumRegisters);
			memset(Keyboard.data(), 0, NumKeys);

			PC = ProgramStartAddress;
			SP = 0;
			I = 0;
			DelayTimer = 0;
			SoundTimer = 0;
			TimeCounter = 0;

			memcpy(Memory.data(), Characters.data(), sizeof(Characters));
		}

		void Load(void * romData, size_t length)
		{
			if (length > ProgramMemory)
			{
				C8_ERRO("Rom data is to big ({0} bytes)", length);
				return;
			}
			Reset();
			memcpy(&Memory[ProgramStartAddress], romData, length);
		}

		void ClockCycle(float deltaTime)
		{
			// Handle timers

			TimeCounter += deltaTime;

			while (TimeCounter > TimePeriod)
			{
				TimeCounter -= TimePeriod;

				if (DelayTimer > 0)
					DelayTimer--;

				if (SoundTimer > 0)
					SoundTimer--;

			}

			// Fetch instruction (big endian)

			uint16_t instruction =  (uint16_t(Memory[PC]) << 8) | Memory[PC + 1];

			PC += 2;

			// Nibbles
			uint8_t n3 = (instruction & 0xf000) >> 12;
			uint8_t n2 = (instruction & 0x0f00) >> 8;
			uint8_t n1 = (instruction & 0x00f0) >> 4;
			uint8_t n0 = (instruction & 0x000f) >> 0;
			
			// High and low byte
			uint8_t hb = (instruction & 0xff00) >> 8;
			uint8_t lb = (instruction & 0x00ff) >> 0;
			
			// Immediate
			int16_t nnn = (instruction & 0x0fff) >> 0;

			// X & Y registers
			uint8_t& vx = V[n2];
			uint8_t& vy = V[n1];

			// Temporary
			uint8_t tmp0[16];
			bool btmp;

			switch (n3)
			{
			case 0:

				switch (lb)
				{
				case 0xe0:
					memset(&Memory[DisplayAddress], 0, TotalMemory - DisplayAddress);
					C8_INFO("CLS");
					break;
				case 0xee:
					PC = Memory[StackAddress + SP] | (uint16_t(Memory[StackAddress + SP + 1]) << 8);
					SP -= 2;
					C8_INFO("RET");
					break;
				default:
					C8_UNKN(instruction, PC);
					PC -= 2;
				}

				break;

			case 1: // Jump
				PC = nnn;
				C8_INFO("JP {0:x}", nnn);
				break;

			case 2: // Call sub
				SP += 2;
				Memory[StackAddress + SP] = PC & 0xff;
				Memory[StackAddress + SP + 1] = (PC & 0xff00) >> 8;
				PC =  nnn;
				C8_INFO("CALL {0:x}", nnn);
				break;

			case 3:
				C8_INFO("SE V{0:x}, {1}", n2, lb);
				if ((vx ^ lb) == 0)
					PC = PC + 2;
				break;
			case 4:
				C8_INFO("SNE V{0:x}, {1}", n2, lb);
				if ((vx ^ lb) != 0)
					PC = PC + 2;
				break;
			case 5:
				C8_INFO("SE V{0:x}, V{1:x}", n2, n1);
				if (vx == vy)
					PC = PC + 2;
				break;
			case 6:
				vx = lb;
				C8_INFO("LD V{0:x}, {1}", n2, lb);
				break;
			case 7:
				vx += lb;
				C8_INFO("ADD V{0:x}, {1}", n2, lb);
				break;
			case 8:

				switch (n0)
				{
				case 0:
					vx = vy;
					C8_INFO("LD V{0:x}, V{1:x}", n2, n1);
					break;
				case 1:
					vx |= vy;
					C8_INFO("OR V{0:x}, V{1:x}", n2, n1);
					break;
				case 2:
					vx &= vy;
					C8_INFO("AND V{0:x}, V{1:x}", n2, n1);
					break;
				case 3:
					vx ^= vy;
					C8_INFO("XOR V{0:x}, V{1:x}", n2, n1);
					break;
				case 4:
					tmp0[0] = vx;
					vx += vy;
					V[15] = vx >= tmp0[0] ? 0 : 1;
					C8_INFO("ADD V{0:x}, V{1:x}", n2, n1);
					break;
				case 5:
					tmp0[0] = vx;
					vx -= vy;
					V[15] = vx > tmp0[0] ? 0 : 1;
					C8_INFO("SUB V{0:x}, V{1:x}", n2, n1);
					break;
				case 6:
					V[15] = vx & 1;
					vx >>= 1;
					C8_INFO("SHR V{0:x}", n2);
					break;
				case 7:
					tmp0[0] = vx;
					vx = vy - vx;
					V[15] = vx > tmp0[0] ? 0 : 1;
					C8_INFO("SUBN V{0:x}, V{1:x}", n2, n1);
					break;
				case 0xe:
					V[15] = (vx & 0x80) >> 7;
					vx <<= 1;
					C8_INFO("SHL V{0:x}", n2);
					break;
				default:
					C8_UNKN(instruction, PC);
					PC -= 2;
				}
				break;

			case 9:
				if (vx != vy)
					PC = PC + 2;
				C8_INFO("SNE V{0:x}, V{1:x}", n2, n1);
				break;

			case 0xa:
				I = nnn;
				C8_INFO("LD I, {0:x}", nnn);
				break;
			case 0xb:
				PC = V[0] + nnn;
				C8_INFO("JP V0, {0:x}", nnn);
				break;
			case 0xc:
				vx = (rand() % 256) & lb;
				C8_INFO("RND V{0:x}, {1}", n2, lb);
				break;
			case 0xd:
				V[15] = 0;
				
				for (uint16_t i = 0; i < n0; i++)
				{
					uint16_t startX = vx;
					uint16_t y = (vy + i) % ScreenHeight;

					uint8_t spriteRow = Memory[I + i];

					for (uint16_t j = 0; j < 8; j++)
					{

						uint16_t x = (startX + j) % ScreenWidth;
						uint16_t bitIdx = y * ScreenWidth + x;

						bool screenVal = VideoMemory.Get(bitIdx);
						bool spriteVal = (spriteRow & (0x80 >> j));

						VideoMemory.Set(bitIdx, screenVal ^ spriteVal);

						if (screenVal && !VideoMemory.Get(bitIdx))
						{
							V[15] = 1;
						}
					}

				}

				C8_INFO("DRW V{0:x}, V{1:x}, {2}", n2, n1, n0);
				break;

			case 0xe:
				switch (lb)
				{
				case 0x9e:
					if (Keyboard[vx])
						PC = PC + 2;
					C8_INFO("SKP V{0:x}", n2);
					break;
				case 0xa1:
					if (!Keyboard[vx])
						PC = PC + 2;
					C8_INFO("SKNP V{0:x}", n2);
					break;
				}

				break;
			case 0xf:
				switch (lb)
				{
				case 0x07:
					vx = DelayTimer;
					C8_INFO("LD V{0:x}, DT", n2);
					break;
				case 0x0a:

					C8_INFO("LD V{0:x}, K", n2);

					btmp = false;
					
					for (uint8_t i = 0; i < sizeof(Keyboard); i++)
					{
						if (Keyboard[i])
						{
							vx = i;
							btmp = true;
							break;
						}
					}

					if (!btmp)
						PC -= 2;


					break;
				case 0x15:
					DelayTimer = vx;
					C8_INFO("LD DT, V{0:x}", n2);
					break;
				case 0x18:
					SoundTimer = vx;
					C8_INFO("LD ST, V{0:x}", n2);
					break;
				case 0x1e:
					I += vx;
					C8_INFO("ADD I, V{0:x}", n2);
					break;
				case 0x29:
					I = vx * 5;
					C8_INFO("LD F, V{0:x}", n2);
					break;
				case 0x33:
					Memory[I + 0] = vx / 100;
					Memory[I + 1] = (vx - Memory[I + 0] * 100) / 10;
					Memory[I + 2] = vx - Memory[I + 0] * 100 - Memory[I + 1] * 10;
					C8_INFO("LD B, V{0:x}", n2);
					break;
				case 0x55:
					for (uint8_t i = 0; i <= n2; i++)
					{
						Memory[I + i] = V[i];
					}
					C8_INFO("LD [I], V{0:x}", n2);
					break;
				case 0x65:
					for (uint8_t i = 0; i <= n2; i++)
					{
						V[i] = Memory[I + i];
					}
					C8_INFO("LD V{0:x}, [I]", n2);
					break;
				default:
					C8_UNKN(instruction, PC);
					PC -= 2;
				}
				break;

			default:
				C8_UNKN(instruction, PC);
				PC -= 2;
				break;
			}
			
			return;

		}
		


	};

	Chip8::Chip8()
	{
		m_Impl = std::make_unique<Impl>();
	}

	Chip8::~Chip8()
	{
	}

	void Chip8::SetPixel(uint8_t x, uint8_t y, bool value)
	{
		assert(x < Impl::ScreenWidth);
		assert(y < Impl::ScreenHeight);
		return m_Impl->VideoMemory.Set(y * Impl::ScreenWidth + x, value);
	}

	bool Chip8::GetPixel(uint8_t x, uint8_t y) const
	{
		assert(x < Impl::ScreenWidth);
		assert(y < Impl::ScreenHeight);
		return m_Impl->VideoMemory.Get(y * Impl::ScreenWidth + x);
	}

	uint8_t Chip8::GetRegister(uint8_t i) const
	{
		assert(i < Impl::NumRegisters);
		return m_Impl->V[i];
	}

	uint8_t Chip8::GetRegisterCount() const
	{
		return uint8_t();
	}

	uint8_t Chip8::GetSoundTimer() const
	{
		return m_Impl->SoundTimer;
	}


	void Chip8::ClockCycle(float deltaTime)
	{
		m_Impl->ClockCycle(deltaTime);
	}

	void Chip8::Load(void * romData, size_t length)
	{
		m_Impl->Load(romData, length);
	}


	void Chip8::SetKeyState(uint8_t key, bool pressed)
	{
		assert(key < Impl::NumKeys);
		m_Impl->Keyboard[key] = pressed;
	}

	uint8_t Chip8::GetScreenWidth() const
	{
		return Impl::ScreenWidth;
	}

	uint8_t Chip8::GetScreenHeight() const
	{
		return Impl::ScreenHeight;
	}



}