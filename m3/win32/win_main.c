/*
 * Sega Model 3 Emulator
 * Copyright (C) 2003 Bart Trzynadlowski, Ville Linde, Stefano Teso
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License Version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program (license.txt); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/******************************************************************/
/* Windows Main													  */
/******************************************************************/

#include "model3.h"

static OSD_CONTROLS controls;
extern BOOL DisassemblePowerPC(UINT32, UINT32, CHAR *, CHAR *, BOOL);


static CHAR app_title[]     = "Model 3 Emulator";
static CHAR class_name[]	= "MODEL3";

HWND main_window;

// Window Procedure prototype
static LRESULT CALLBACK win_window_proc(HWND, UINT, WPARAM, LPARAM);

BOOL win_register_class(void)
{
	WNDCLASSEX wcex;

	wcex.cbSize			= sizeof(WNDCLASSEX);
	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)win_window_proc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= GetModuleHandle(NULL);
	wcex.hIcon			= NULL;
	wcex.hIconSm		= NULL;
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)GetStockObject(BLACK_BRUSH);
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= class_name;

	if( FAILED(RegisterClassEx(&wcex)) ) // MinGW: "comparison is always false due to limited range of data"
		return FALSE;

	return TRUE;
}

BOOL win_create_window(void)
{
	DWORD frame_width, frame_height, caption_height;
	int width, height, window_width, window_height;

	if(!m3_config.fullscreen) {
		window_width	= MODEL3_SCREEN_WIDTH;
		window_height	= MODEL3_SCREEN_HEIGHT;
	} else {
		window_width	= m3_config.width;
		window_height	= m3_config.height;
	}

	frame_width			= GetSystemMetrics(SM_CXSIZEFRAME);
	frame_height		= GetSystemMetrics(SM_CYSIZEFRAME);
	caption_height		= GetSystemMetrics(SM_CYCAPTION);

	width	= (window_width - 1) + (frame_width * 2);
	height	= (window_height - 1) + (frame_height * 2) + caption_height;

	main_window = CreateWindow(class_name,
							   app_title,
                               WS_CLIPSIBLINGS | WS_CLIPCHILDREN |  // required for OpenGL
							   WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX,
							   CW_USEDEFAULT, CW_USEDEFAULT,	// Window X & Y coords
							   width - 1, height - 1,			// Width & Height
							   NULL, NULL,						// Parent Window & Menu
							   GetModuleHandle(NULL), NULL );

	if(!main_window)
		return FALSE;

	return TRUE;
}

int main(int argc, char *argv[])
{
    CHAR    mnem[16], oprs[48];
    MSG     msg;
    BOOL    quit = FALSE;
    FILE    *fp;
    UINT    i, frame = 0, op;

	if(argc < 2) {
		// Show usage
		printf("ERROR: not enough arguments.\n\n");
		printf("Usage: m3.exe [romset]\n");
		return 0;
	}

	// Load config

    m3_config.layer_enable = 0xF;
    memset(controls.system_controls, 0xFF, 2);
    memset(controls.game_controls, 0xFF, 2);

	// Parse command-line

    strncpy(m3_config.game_id, argv[1], 8);
    m3_config.game_id[8] = '\0';    // in case game name was 8 or more chars

	// Some initialization

	if(!win_register_class()) {
		printf("win_register_class failed.");
		return 0;
	}
	if(!win_create_window()) {
		printf("win_create_window failed.");
		return 0;
	}

    m3_init();
	m3_reset();

	// Now that everything works, we can show the window

	ShowWindow(main_window, SW_SHOWNORMAL);
    SetForegroundWindow(main_window);
    SetFocus(main_window);
	UpdateWindow(main_window);

	memset(&msg, 0, sizeof(MSG));
    while (quit == FALSE)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
                quit = TRUE;
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

        printf("frame %d, PC = %08X, ", frame++, ppc_get_reg(PPC_REG_PC));
        op = ppc_read_word(ppc_get_reg(PPC_REG_PC));
        if (DisassemblePowerPC(op, ppc_get_reg(PPC_REG_PC), mnem, oprs, 1))
        {
            if (mnem[0] != '\0')    // invalid form
                printf("%08X %s*\t%s\n", op, mnem, oprs);
            else
                printf("%08X ?\n", op);
        }
        else
            printf("%08X %s\t%s\n", op, mnem, oprs);

        m3_run_frame();

	}

    for (i = 0; i < 32; i += 4)
        printf("R%d=%08X\tR%d=%08X\tR%d=%08X\tR%d=%08X\n",
        i + 0, ppc_get_reg(PPC_REG_R0 + i + 0),
        i + 1, ppc_get_reg(PPC_REG_R0 + i + 1),
        i + 2, ppc_get_reg(PPC_REG_R0 + i + 2),
        i + 3, ppc_get_reg(PPC_REG_R0 + i + 3));
	exit(0);
}

void osd_warning()
{

}

void osd_error(CHAR * string)
{
	printf("ERROR: %s\n",string);

	exit(0);

	/* terminate emulation, renderer, input etc. */
	/* revert to plain GUI. */
}

OSD_CONTROLS * osd_input_update_controls(void)
{
    return &controls;
}

static LRESULT CALLBACK win_window_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    CHAR    fname[13];

	switch(message)
	{
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
        case WM_LBUTTONDOWN:
            controls.game_controls[0] &= ~0x01;
            break;
        case WM_LBUTTONUP:
            controls.game_controls[0] |= 0x01;
            break;
        case WM_MOUSEMOVE:
            controls.gun_x[0] = LOWORD(lParam);
            controls.gun_y[0] = HIWORD(lParam);
            return 0;
		case WM_KEYDOWN:
            switch (wParam)
            {
            /*
             * NOTE: VK_ macros are not defined for most keys, it seems, so
             * I hardcoded these:
             *
             * Up,Down,Left,Right = 26,28,25,27
             * Num 1,Num 3        = 23,22
             * A,S,D,F,G,H        = 41,53,44,46,47,48
             * Z,X,C,V,B          = 5A,58,43,56,42
             */

            default:
                printf("KEY = %02X\n", wParam);
                break;

            case 0x41:  // A -- for VON2, jump
                controls.game_controls[0] &= ~0x80;
                controls.game_controls[1] &= ~0x40;
                break;
            case 0x53:  // S -- for VON2, crouch ?? (joysticks inward)
                controls.game_controls[0] &= ~0x40;
                controls.game_controls[1] &= ~0x80;
                break;
            case 0x5A:  // Z -- for VON2, shot trigger 1
                controls.game_controls[0] &= ~0x01;
                break;
            case 0x58:  // X -- for VON2, shot trigger 2
                controls.game_controls[1] &= ~0x01;
                break;
            case 0x43:  // C -- for VON2, turbo 1
                controls.game_controls[0] &= ~0x02;
                break;
            case 0x56:  // V -- for VON2, turbo 2
                controls.game_controls[1] &= ~0x02;
                break;
            case 0x26:  // Up arrow -- for VON2, move forward
                controls.game_controls[0] &= ~0x20;
                controls.game_controls[1] &= ~0x20;
                break;
            case 0x28:  // Down arrow -- for VON2, move backward
                controls.game_controls[0] &= ~0x10;
                controls.game_controls[1] &= ~0x10;
                break;
            case 0x25:  // Left arrow -- for VON2, turn left
                controls.game_controls[0] &= ~0x10;
                controls.game_controls[1] &= ~0x20;
                break;
            case 0x27:  // Right arrow -- for VON2, turn right
                controls.game_controls[0] &= ~0x20;
                controls.game_controls[1] &= ~0x10;
                break;
            case 0x23:  // Numpad 1 -- for VON2, strafe left
                controls.game_controls[0] &= ~0x80;
                controls.game_controls[1] &= ~0x80;
                break;
            case 0x22:  // Numpad 3 -- for VON2, strafe right
                controls.game_controls[0] &= ~0x40;
                controls.game_controls[1] &= ~0x40;
                break;
            case VK_F12:
                m3_config.layer_enable ^= 1;
                break;
            case VK_F11:
                m3_config.layer_enable ^= 2;
                break;
            case VK_F10:
                m3_config.layer_enable ^= 4;
                break;
            case VK_F9:
                m3_config.layer_enable ^= 8;
                break;
            case VK_F8:
                m3_config.layer_enable = 0xF;
                break;
            case VK_ESCAPE:
                DestroyWindow(hWnd);
                break;
            case VK_F5:
                strncpy(fname, m3_config.game_id, 8);
                fname[8] = '\0';
                strcat(fname, ".sta");
                m3_save_state(fname);
                break;
            case VK_F7:
                strncpy(fname, m3_config.game_id, 8);
                fname[8] = '\0';
                strcat(fname, ".sta");
                m3_load_state(fname);
                break;
            case VK_F1: // Test
                controls.system_controls[0] &= ~0x04;
                controls.system_controls[1] &= ~0x04;
                break;
            case VK_F2:  // Service
                controls.system_controls[0] &= ~0x08;
                controls.system_controls[1] &= ~0x80;
                break;
            case VK_F3: // Start
                controls.system_controls[0] &= ~0x10;
                break;
            case VK_F4: // Coin #1
                controls.system_controls[0] &= ~0x01;
                break;
            }
			break;
        case WM_KEYUP:
            switch (wParam)
            {
            case 0x41:  // A -- for VON2, jump
                controls.game_controls[0] |= 0x80;
                controls.game_controls[1] |= 0x40;
                break;
            case 0x53:  // S -- for VON2, crouch
                controls.game_controls[0] |= 0x40;
                controls.game_controls[1] |= 0x80;
                break;
            case 0x5A:  // Z -- for VON2, shot trigger 1
                controls.game_controls[0] |= 0x01;
                break;
            case 0x58:  // X -- for VON2, shot trigger 2
                controls.game_controls[1] |= 0x01;
                break;
            case 0x43:  // C -- for VON2, turbo 1
                controls.game_controls[0] |= 0x02;
                break;
            case 0x56:  // V -- for VON2, turbo 2
                controls.game_controls[1] |= 0x02;
                break;
            case 0x26:  // Up arrow -- for VON2, move forward
                controls.game_controls[0] |= 0x20;
                controls.game_controls[1] |= 0x20;
                break;
            case 0x28:  // Down arrow -- for VON2, move backward
                controls.game_controls[0] |= 0x10;
                controls.game_controls[1] |= 0x10;
                break;
            case 0x25:  // Left arrow -- for VON2, turn left
                controls.game_controls[0] |= 0x10;
                controls.game_controls[1] |= 0x20;
                break;
            case 0x27:  // Right arrow -- for VON2, turn right
                controls.game_controls[0] |= 0x20;
                controls.game_controls[1] |= 0x10;
                break;
            case 0x23:  // Numpad 1 -- for VON2, strafe left
                controls.game_controls[0] |= 0x80;
                controls.game_controls[1] |= 0x80;
                break;
            case 0x22:  // Numpad 3 -- for VON2, strafe right
                controls.game_controls[0] |= 0x40;
                controls.game_controls[1] |= 0x40;
                break;

            case VK_F1:
                controls.system_controls[0] |= 0x04;
                controls.system_controls[1] |= 0x04;
                break;
            case VK_F2:
                controls.system_controls[0] |= 0x08;
                controls.system_controls[1] |= 0x80;
                break;
            case VK_F3:
                controls.system_controls[0] |= 0x10;
                break;
            case VK_F4:
                controls.system_controls[0] |= 0x01;
                break;
            }
            break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
