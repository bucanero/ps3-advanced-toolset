/* 
   TINY3D sample / (c) 2010 Hermes  <www.elotrolado.net>

   PS3LoadX is the evolution of PSL1GHT PS3Load sample

*/

#include <sys/process.h>
#include <sys/file.h>
#include <sys/stat.h>

#include <sys/thread.h>
#include <sysmodule/sysmodule.h>
#include <sysutil/sysutil.h>

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <math.h>

#include <fcntl.h>
#include <unistd.h>
#include <soundlib/gcmodplay.h>
#include <soundlib/spu_soundlib.h>

#include "gfx.h"
#include "pad.h"
#include "spu_soundmodule_bin.h"
#include "space_debris_mod_bin.h"

int v_release = 0;

char msg_error[128];
//char msg_one  [128];
char msg_two  [128];

char bootpath[MAXPATHLEN];

#define RGBA(r, g, b, a) (((r) << 24) | ((g) << 16) | ((b) << 8) | (a))

void release_all();

#define DT_DIR 1

#define VERSION "v1.0.0"
#define MAX_ARG_COUNT 0x100

#define ERROR(a, msg) { \
	if (a < 0) { \
		snprintf(msg_error, sizeof(msg_error), "PS3LoadX: " msg ); \
        usleep(250); \
	} \
}
#define ERROR2(a, msg) { \
	if (a < 0) { \
		snprintf(msg_error, sizeof(msg_error), "PS3LoadX: %s", msg ); \
        sleep(2); \
        msg_error[0] = 0; \
		usleep(60); \
		goto reloop; \
	} \
}
#define MIN(a, b) ((a) < (b) ? (a) : (b))

char ps3load_path[MAXPATHLEN]= "/dev_hdd0/game/PS3T000LZ/TOOLS";
char install_folder[MAXPATHLEN];

MODPlay mod_track;   // struct for the MOD Player

// SPU
u32 inited;
u32 spu = 0;
sysSpuImage spu_image;

#define INITED_CALLBACK     1
#define INITED_SPU          2
#define INITED_SOUNDLIB     4
#define INITED_MODPLAYER    8

#define SPU_SIZE(x) (((x)+127) & ~127)


void InitSoundlib()
{
	//Initialize SPU
	u32 entry = 0;
	u32 segmentcount = 0;
	sysSpuSegment* segments;
	
	sysSpuInitialize(6, 5);
	sysSpuRawCreate(&spu, NULL);
	sysSpuElfGetInformation(spu_soundmodule_bin, &entry, &segmentcount);
	
	size_t segmentsize = sizeof(sysSpuSegment) * segmentcount;
	segments = (sysSpuSegment*)memalign(128, SPU_SIZE(segmentsize)); // must be aligned to 128 or it break malloc() allocations
	memset(segments, 0, segmentsize);

	sysSpuElfGetSegments(spu_soundmodule_bin, segments, segmentcount);
	sysSpuImageImport(&spu_image, spu_soundmodule_bin, 0);
	sysSpuRawImageLoad(spu, &spu_image);
	
	inited |= INITED_SPU;
	if(SND_Init(spu)==0)
		inited |= INITED_SOUNDLIB;
}

void PlayModTrack()
{
	InitSoundlib(); // Initialize the Sound Lib
	
	if (!(inited & INITED_SOUNDLIB))
		return;

	MODPlay_Init(&mod_track);  // Initialize the MOD Library

	if (MODPlay_SetMOD (&mod_track, space_debris_mod_bin ) < 0) // set the MOD song
    {
        MODPlay_Unload (&mod_track);
        return;
    }

    SND_Pause(0); // the sound loop is running now

    MODPlay_SetVolume( &mod_track, 64,64); // fix the volume to 64 (max 64)
    MODPlay_Start (&mod_track); // Play the MOD

	inited |= INITED_MODPLAYER;
}


// thread

int running = 1;

volatile int flag_exit=0;

struct {
    
    int text;
    int device;
    char name[MAXPATHLEN+1];
    char title[40+1];

} directories[256];

int menu_level    = 0;
int pendrive_test = 0;
int hdd_test      = 0;
int device_mode   = 0;
int ndirectories  = 0;
int curdir = 0;

volatile int hdd_install = 0;

char filename [1024];
char filename2[1024];

u32 color_two = 0xffffffff;

static void control_thread(void* arg)
{
	
	int i;
    float x=0, y=0;
    static u32 C1, C2, C3, C4, count_frame = 0;
    
    int yesno =0;
	
	while(running ){
       
        ps3pad_read();

        if((new_pad & BUTTON_CIRCLE) && !menu_level){
			
            menu_level = 2; yesno = 0;
		}

        if((new_pad & BUTTON_SQUARE) && !menu_level  && ndirectories>0){
			
            menu_level = 3; yesno = 0;	
		}

        if(ndirectories <= 0 && (menu_level==1)) menu_level = 0;

        if((new_pad & BUTTON_CROSS)){

                switch(menu_level)
                {
                case 0:
                    
                    if(ndirectories>0) {menu_level = 1; yesno = 0;}

                break;
                
                // launch from device
                case 1:

                    if(yesno) {

                        flag_exit = 2;

                        if(directories[curdir].device)
                            sprintf(bootpath, "/dev_usb000/PS3T000LZ/%s/tool.self", &directories[curdir].name[0]);
                        else
                            sprintf(bootpath, "%s/%s/tool.self", ps3load_path, &directories[curdir].name[0]);
                    

                    } else menu_level = 0;

                break;
                
                // exit
                case 2:

                    if(yesno) {
                        flag_exit = 1;
                        

                    } else menu_level = 0;

                break;

                // Disable music
                case 3:

                    if(yesno) {
                       
                       yesno =0;
                       SND_Pause(1);
                       menu_level  = 0;

                    } else {menu_level = 0; SND_Pause(0);}

                break;

                }
	
		    }

         if(ndirectories<=0 && (menu_level== 2 || menu_level== 5 || menu_level== 6)) {
            
            if((new_pad & BUTTON_LEFT))  yesno = 1;
            if((new_pad & BUTTON_RIGHT)) yesno = 0;

         }

        if(ndirectories > 0) {
            static int b_left = 0, b_right = 0;

         
            if(menu_level) {b_left = b_right = 0;}

            if((old_pad & BUTTON_LEFT) && b_left) {
                b_left++;
                if(b_left > 15) {b_left = 1; new_pad |= BUTTON_LEFT;}
            } else b_left = 0;

            if((old_pad & BUTTON_RIGHT) && b_right) {
                b_right++;
                if(b_right > 15) {b_right = 1; new_pad |= BUTTON_RIGHT;}
            } else b_right = 0;

            if(menu_level) {
                if((new_pad & BUTTON_LEFT))  yesno = 1;
                if((new_pad & BUTTON_RIGHT)) yesno = 0;
            } else {
                if((new_pad & BUTTON_LEFT)) {

                    int next_dir;
                    b_left = 1;
                    next_dir = ((u32)(ndirectories + curdir + 1)) % ndirectories;
                    
                    directories[next_dir].text = -1;

                    next_dir = ((u32)(ndirectories + curdir - 1)) % ndirectories;
                        
                    directories[curdir].text = -1;
                    directories[next_dir].text = -1;
                    
                    curdir = next_dir;
                    next_dir = ((u32)(ndirectories + curdir - 1)) % ndirectories;
                    
                    directories[next_dir].text = -1;

                }

                if((new_pad & BUTTON_RIGHT)) {

                    int next_dir;
                    b_right = 1;
                    next_dir = ((u32)(ndirectories + curdir - 1)) % ndirectories;
                    
                    directories[next_dir].text = -1;

                    next_dir = ((u32)(ndirectories + curdir + 1)) % ndirectories;
                        
                    directories[curdir].text = -1;
                    directories[next_dir].text = -1;
                    
                    curdir = next_dir;
                    next_dir = ((u32)(ndirectories + curdir + 1)) % ndirectories;
                    
                    directories[next_dir].text = -1;

                }
            
            }
        }


        tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);

        // Enable alpha Test
        tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);

        // Enable alpha blending.
        tiny3d_BlendFunc(1, TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
            TINY3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA | TINY3D_BLEND_FUNC_DST_ALPHA_ZERO,
            TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);

        
        C1=RGBA(0, 0x30, 0x80 + (count_frame & 0x7f), 0xff);  
        C2=RGBA(0x80 + (count_frame & 0x7f), 0, 0xff, 0xff);
        C3=RGBA(0,  (count_frame & 0x7f)/4, 0xff - (count_frame & 0x7f), 0xff);
        C4=RGBA(0, 0x80 - (count_frame & 0x7f), 0xff, 0xff);
        
        
        if(count_frame == 127) count_frame = 255;
        if(count_frame == 128) count_frame = 0;
        
        if(count_frame & 128) count_frame--;
        else count_frame++;

        tiny3d_SetPolygon(TINY3D_QUADS);

        tiny3d_VertexPos(0  , 0  , 65535);
        tiny3d_VertexColor(C1);

        tiny3d_VertexPos(847, 0  , 65535);
        tiny3d_VertexColor(C2);

        tiny3d_VertexPos(847, 511, 65535);
        tiny3d_VertexColor(C3);

        tiny3d_VertexPos(0  , 511, 65535);
        tiny3d_VertexColor(C4);


        tiny3d_End();

        update_twat();
    
        if(jpg1.bmp_out) {

            float x2=  ((float) ( (int)(count_frame & 127)- 63)) / 42.666f;
			
			// calculate coordinates for JPG

            x=(848-jpg1.width*2)/2; y=(512-jpg1.height*2)/2;

            tiny3d_SetTexture(0, jpg1_offset, jpg1.width, jpg1.height, jpg1.pitch, TINY3D_TEX_FORMAT_A8R8G8B8, 1);

            tiny3d_SetPolygon(TINY3D_QUADS);
            
            
            tiny3d_VertexPos(x + x2            , y +x2             , 65534);
            tiny3d_VertexColor(0xffffff10);
            tiny3d_VertexTexture(0.0f , 0.0f);

            tiny3d_VertexPos(x - x2 + jpg1.width*2, y +x2              , 65534);
            tiny3d_VertexTexture(0.99f, 0.0f);

            tiny3d_VertexPos(x + x2 + jpg1.width*2, y -x2+ jpg1.height*2, 65534);
            tiny3d_VertexTexture(0.99f, 0.99f);

            tiny3d_VertexPos(x - x2           , y -x2+ jpg1.height*2, 65534);
            tiny3d_VertexTexture(0.0f , 0.99f);

            tiny3d_End();
		
		}
 
         
        for(i = 0; i< 3; i++) {
            int index;
            
            float w, h, z = (i==1) ? 1 : 2;
            float scale = (i==1) ? 128 : 96;
            
            x=0; y=0;
            
            
            if(ndirectories > 0) index = ((u32) (ndirectories + curdir - 1 + i)) % ndirectories; else index = 0;


        // draw PNG
        
            if(ndirectories > 0 && directories[index].text >= 0 && Png_datas[directories[index].text].bmp_out) {
            
                x=(848 - scale * Png_datas[directories[index].text].width / Png_datas[directories[index].text].height) / 2 - 256 + 256 * i;
                y=(512- scale)/2;

                w = scale * Png_datas[directories[index].text].width / Png_datas[directories[index].text].height;
                h = scale;


                tiny3d_SetTexture(0, Png_offset[directories[index].text], Png_datas[directories[index].text].width, 
                    Png_datas[directories[index].text].height, Png_datas[directories[index].text].pitch, TINY3D_TEX_FORMAT_A8R8G8B8, 1);

                
               
                if(directories[index].text == 4)
                    DrawTextBox(x, y, z, w, h, 0xffffff60);
                else
                    DrawTextBox(x, y, z, w, h, 0xffffffff);
            
            } else {

                x=(848 - scale * 2/1) / 2 - 256 + 256 * i;
                y=(512- scale)/2;

                w = scale * 2/1;
                h = scale;

                DrawBox(x, y, z, w, h, 0x80008060);
            
            }
        }
        

        SetFontSize(16, 32);
        x=0; y=0;
        
        SetFontColor(0xFFFF00FF, 0x00000000);
        SetFontAutoCenter(1);
        
        if(!v_release) DrawString(x, y, "PS3 Advanced Toolset " VERSION " - PSL1GHT TEST"); 
        else DrawString(x, y, "PS3 Advanced Toolset " VERSION " - PSL1GHT");
        
        SetFontAutoCenter(0);

        SetFontSize(12, 24);

        SetFontColor(0xFFFFFFFF, 0x00000000);

        y += 24 * 4;
        
        SetFontSize(24, 48);
        SetFontAutoCenter(1);
        
        if(device_mode) DrawString(0, y, "USB Applications"); else DrawString(0, y, "HDD Applications");

        SetFontAutoCenter(0);

        SetFontSize(16, 32);

        if(ndirectories > 0) {

            SetFontAutoCenter(1);
            x= 0.0; y = 336;
            SetFontColor(0xFFFFFFFF, 0x80008050);

            if(directories[curdir].title[0] != 0)
                DrawFormatString(x, y, "%s", &directories[curdir].title[0]);
            else
                DrawFormatString(x, y, "%s", &directories[curdir].name[0]);

            SetFontAutoCenter(0);
        }

        
        SetFontSize(12, 24);
        SetFontAutoCenter(1);
        x= 0.0; y = 512 - 48;
        if(msg_error[0]!=0) {
            SetFontColor(0xFF3F00FF, 0x00000050);
            DrawFormatString(x, y, "%s", msg_error);
        }
        else {
            SetFontColor(color_two, 0x00000050);
            DrawFormatString(x, y, "%s", msg_two);
        }

        SetFontAutoCenter(0);

        SetFontColor(0xFFFFFFFF, 0x00000000);

        if(menu_level) {

            x= (848-640) / 2; y=(512-360)/2;

           // DrawBox(x, y, 0.0f, 640.0f, 360, 0x006fafe8);
            DrawBox(x - 16, y - 16, 0.0f, 640.0f + 32, 360 + 32, 0x00000038);
            DrawBox(x, y, 0.0f, 640.0f, 360, 0x300030d8);

            SetFontSize(16, 32);
            SetFontAutoCenter(1);
            
            y += 32;
            
            switch(menu_level) {
            
            case 1:
                DrawString(0, y, "Launch Application?");
            break;
            
            case 2:
                DrawString(0, y, "Exit to PS3 Menu?");
            break;

            case 3:
                DrawString(0, y, "Disable Music?");
            break;

            }

            SetFontAutoCenter(0);
            
            y += 100;

            x = 300;

            DrawBox(x, y, 0.0f, 5*16, 32*3, 0x5f00afff);
            
            if(yesno) SetFontColor(0xFFFFFFFF, 0x00000000); else SetFontColor(0x606060FF, 0x00000000);
            
            DrawString(x+16, y+32, "YES");

            x = 848 - 300- 5*16;

            DrawBox(x, y, 0.0f, 5*16, 32*3, 0x5f00afff);
            if(!yesno) SetFontColor(0xFFFFFFFF, 0x00000000); else SetFontColor(0x606060FF, 0x00000000);
            
            DrawString(x+24, y+32, "NO");
		
        }

		sysThreadYield();
		
		tiny3d_Flip();
		sysUtilCheckCallback();
		
		
	}
	//you must call this, kthx
	sysThreadExit(0);
}

static void file_thread(void* arg)
{
    int i;
    
    int counter2=0;
    int n;

    s32 dir;
    FILE *fp;

    while(running ){

        if((counter2 & 31)==0) {
            int refresh = 0;

            if(sysLv2FsOpenDir("/dev_usb000/PS3T000LZ/", &dir) == 0) {
                if(!pendrive_test) {hdd_test = 0; device_mode = 1; refresh = 1;} else sysLv2FsCloseDir(dir);
            } else {

                if(device_mode == 0 && sysLv2FsOpenDir("/dev_usb000/", &dir) == 0) {
                    mkdir("/dev_usb000/PS3T000LZ", 0777);
                   sysLv2FsCloseDir(dir);
                   hdd_test = 0; pendrive_test = 0;
                   ndirectories = 0;
                   curdir = 0;
                   device_mode = 1;
                   continue;
                }

                device_mode = 0;
                pendrive_test = 0;
                if(sysLv2FsOpenDir(ps3load_path, &dir) == 0) {
                    if(!hdd_test) {device_mode = 0; refresh = 1;} else sysLv2FsCloseDir(dir);
                } else {
                    ndirectories = 0;
                    curdir = 0;
                }
            }
                

            if(refresh) {
                ndirectories = 0; curdir = 0;
                n = 0;

                while(1) {
                    u64 read = sizeof(sysFSDirent);
                    sysFSDirent entry;
                    directories[n].text = -1;

                    if(sysLv2FsReadDir(dir, &entry, &read)<0 || read <= 0) break;

                    if((entry.d_type & 1) && entry.d_name[0] != '.') {
                        strcpy(&directories[n].name[0], entry.d_name);
                        directories[n].title[0] = 0;

                        if(device_mode)
                            sprintf(filename, "/dev_usb000/PS3T000LZ/%s/title.txt", &directories[n].name[0]);
                        else
                            sprintf(filename, "%s/%s/title.txt", ps3load_path, &directories[n].name[0]);
                       

                        fp =fopen(filename, "r");
                        if(fp){
                            if(!fgets(&directories[n].title[0], 40, fp)) directories[n].title[0] = 0;
                            fclose(fp);
                        }
                        i=0; 
                        while(directories[n].title[i] && i < 40) {
                            if(directories[n].title[i] == 13 || directories[n].title[i] == 10)  break;
                            if(directories[n].title[i] < 32) directories[n].title[i] = 32;
                            i++;
                        }
                        directories[n].device = device_mode;
                        n++;
                    }
                    
                }
                
                

                ndirectories = n;
            }
            if(device_mode) pendrive_test = 1; else hdd_test = 1;

            sysLv2FsCloseDir(dir);
        } 


        if(ndirectories > 0) {
            for(i = 0; i< 3; i++) {
                int index = ((u32) (ndirectories + curdir - 1 + i)) % ndirectories;

                // LOAD PNG
                
                if(ndirectories && directories[index].text<0) {
                    
                    if(directories[index].device)
                        sprintf(filename, "/dev_usb000/PS3T000LZ/%s/ICON0.PNG", &directories[index].name[0]);
                    else
                        sprintf(filename, "%s/%s/ICON0.PNG", ps3load_path, &directories[index].name[0]);

                    if(LoadTexturePNG(filename, i) == 0) directories[index].text = i;
                    else {
                        directories[index].text = 4;
                        memcpy(&Png_datas[4], &jpg1, sizeof(jpgData));
                        Png_offset[4] = jpg1_offset;
                    }
                }
            }
        }


        counter2++;

        usleep(20000);

	}
	//you must call this, kthx
	sysThreadExit(0);
}



static void sys_callback(uint64_t status, uint64_t param, void* userdata) {

     switch (status) {
		case SYSUTIL_EXIT_GAME:
			flag_exit=1;
			release_all();
			sysProcessExit(1);
			break;
      
       default:
		   break;
         
	}
}

sys_ppu_thread_t thread1_id, thread2_id;

void release_all() {

	u64 retval;

	if (inited & INITED_MODPLAYER)
	{
		MODPlay_Unload (&mod_track);
		SND_End();
	}

	sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
	running= 0;
	sysThreadJoin(thread1_id, &retval);
    sysThreadJoin(thread2_id, &retval);

	sysModuleUnload(SYSMODULE_JPGDEC);
    sysModuleUnload(SYSMODULE_PNGDEC);
    sysModuleUnload(SYSMODULE_FS);

}


int main(int argc, const char* argv[], const char* envp[])
{
	sysModuleLoad(SYSMODULE_FS);
    sysModuleLoad(SYSMODULE_PNGDEC);
	sysModuleLoad(SYSMODULE_JPGDEC);

    if(argc>0 && argv) {
    
        if(!strncmp(argv[0], "/dev_hdd0/game/", 15)) {
            int n;

            strcpy(ps3load_path, argv[0]);

            n= 15; while(ps3load_path[n] != '/' && ps3load_path[n] != 0) n++;

            if(ps3load_path[n] == '/') {
                sprintf(&ps3load_path[n], "%s", "/RELOAD.SELF");
                sysLv2FsChmod(ps3load_path, 0170777ULL);

                sprintf(&ps3load_path[n], "%s", "/TOOLS");
                v_release = 1;
            }
        }
    }

	msg_error[0] = 0; // clear msg_error
    //msg_one  [0] = 0;
    msg_two  [0] = 0;

    tiny3d_Init(1024*1024);
    
    LoadTexture();
    init_twat();

	ioPadInit(7);
	

    sysThreadCreate( &thread1_id, control_thread, 0ULL, 999, 256*1024, THREAD_JOINABLE, "Control Thread ps3load");
    sysThreadCreate( &thread2_id, file_thread, 0ULL, 1000, 256*1024, THREAD_JOINABLE, "File Thread ps3load");

	// register exit callback
	sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sys_callback, NULL);

    PlayModTrack();

#define continueloop() { goto reloop; }

reloop:

    msg_two[0]   = 0;

	while (1) {
		
        usleep(20000);
		
        if(flag_exit) break;

        color_two = 0xffffffff;

		sprintf(msg_two, "Waiting for connection...");

		if(flag_exit) break;
        
		continueloop();

	}

    if(flag_exit == 2) {
        release_all();
		sleep(1);
        sysProcessExitSpawn2(bootpath, NULL, NULL, NULL, 0, 1001, SYS_PROCESS_SPAWN_STACK_SIZE_1M);
    }
	sprintf(msg_two, "Exiting...");
    usleep(250);
	release_all();

	sleep(2);
	return 0;
}
