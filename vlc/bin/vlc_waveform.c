/* SPDX-License-Identifier: GPL-2.0-or-later
 * Local waveform extraction using this build's unchanged LibVLC decoders.
 * Separate from the interactive playlist; no rendering or audible output.
 */
#include <vlc/vlc.h>
#include <vlc/libvlc_parser.h>
#include <vlc/libvlc_picture.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static void media_stopping(void *opaque, libvlc_media_t *media, libvlc_stopping_reason_t reason)
{
    (void)media;
    atomic_store((atomic_int *)opaque, reason==libvlc_stopping_reason_eos ? 0 : 6);
}

struct preview_context { const char *output; atomic_int result; };
static void preview_ended(void *opaque, libvlc_parser_task *task, libvlc_picture_t *picture)
{
    (void)task;
    struct preview_context *context=opaque;
    atomic_store(&context->result, picture && libvlc_picture_save(picture,context->output)==0 ? 0 : 6);
}

static int preview(const char *source, const char *output, const char *time)
{
    if (strncmp(source,"file:///",8) || !time) return 2;
    char *end;
    errno=0;
    const double seconds=strtod(time,&end);
    if (errno || *end || !isfinite(seconds) || seconds<0 || seconds>864000) return 2;
    const char *options[]={"--ignore-config","--no-media-library","--no-one-instance","--no-metadata-network-access","--aout=dummy","--vout=dummy","--quiet"};
    libvlc_instance_t *instance=libvlc_new(sizeof(options)/sizeof(options[0]),options);
    if (!instance) return 4;
    libvlc_media_t *media=libvlc_media_new_location(source);
    if (media) libvlc_media_add_option_flag(media,"avcodec-threads=1",libvlc_media_option_trusted);
    const struct libvlc_parser_cfg configuration={.version=0,.max_parser_threads=1,.max_thumbnailer_threads=1,.timeout=8000000};
    libvlc_parser_t *parser=media ? libvlc_parser_new(instance,&configuration) : NULL;
    struct preview_context context={.output=output,.result=ATOMIC_VAR_INIT(-1)};
    const struct libvlc_thumbnailer_cbs callbacks={.version=0,.on_ended=preview_ended};
    const libvlc_thumbnailer_request_t request={.version=0,.media=media,.width=208,.height=0,.type=libvlc_picture_Png,
        .seek={.type=libvlc_thumbnailer_seek_time,.value={.time=(libvlc_time_t)(seconds*1000000)},.speed=libvlc_media_thumbnail_seek_precise}};
    libvlc_parser_task *task=parser ? libvlc_parser_task_new_thumbnail(parser,&request,&callbacks,&context) : NULL;
    if (task && libvlc_parser_submit(parser,task)==0) {
        for (int step=0; step<900 && atomic_load(&context.result)==-1; ++step) {
#ifdef _WIN32
            Sleep(10);
#else
            usleep(10000);
#endif
        }
    }
    if (parser) libvlc_parser_destroy(parser);
    if (task) libvlc_parser_task_release(task);
    if (media) libvlc_media_release(media);
    libvlc_release(instance);
    return atomic_load(&context.result)==0 ? 0 : 6;
}

static int extract(const char *source, const char *output, const char *kind, const char *track)
{
    if (kind && !strcmp(kind,"preview")) return preview(source,output,track);
    const int subtitles = kind != NULL;
    if (subtitles && strcmp(kind,"subtitles-ass") && strcmp(kind,"subtitles-text")) return 2;
    const int diagnostic=getenv("VLC_REDESIGN_HELPER_DIAGNOSTIC")!=NULL;
    if (diagnostic) { fprintf(stderr,"waveform: enter\n"); fflush(stderr); }
    if (strncmp(source,"file:///",8) != 0 || strchr(output,'"')) return 2;
    size_t size=strlen(output)+384;
    char *chain=malloc(size);
    if (!chain) return 3;
    if (!subtitles)
        snprintf(chain,size,"--sout=#transcode{acodec=s16l,samplerate=8000,channels=1,threads=1}:std{access=file,mux=raw,dst=\"%s\"}",output);
    else {
        const char *id = track ? strrchr(track,'/') : NULL;
        char selection[48]="spu";
        if (track && *track) {
            char *end;
            if (!id || !id[1]) { free(chain); return 2; }
            errno=0;
            long number=strtol(id+1,&end,10);
            if (errno || *end || number<0 || number>INT_MAX) { free(chain); return 2; }
            snprintf(selection,sizeof(selection),"es=%ld",number);
        }
        if (!strcmp(kind,"subtitles-ass"))
            snprintf(chain,size,"--sout=#duplicate{dst=std{access=file,mux=avformat{mux=ass},dst=\"%s\"},select=\"%s\"}",output,selection);
        else
            snprintf(chain,size,"--sout=#duplicate{dst=std{access=file,mux=webvtt,dst=\"%s\"},select=\"%s\"}",output,selection);
    }
    const char *options[16]={"--ignore-config","--no-media-library","--no-one-instance","--no-video","--aout=dummy","--vout=dummy",diagnostic ? "--verbose=2" : "--quiet",chain};
    int count=8;
    options[count++]=subtitles ? "--no-sout-audio" : "--no-spu";
    options[count++]=subtitles ? "--sout-video" : "--demux=avformat";
    options[count++]=subtitles ? "--sout-spu" : "--stop-time=7200";
    if (subtitles) options[count++]="--no-audio";
    if (subtitles) options[count++]="--sout-all";
    libvlc_instance_t *instance=libvlc_new(count,options);
    if (diagnostic) { fprintf(stderr,"waveform: instance=%d\n",instance!=NULL); fflush(stderr); }
    if (!instance) { free(chain); return 4; }
    if (diagnostic) libvlc_log_set_file(instance, stderr);
    libvlc_media_t *media=libvlc_media_new_location(source);
    if (media) libvlc_media_add_option_flag(media,chain+2,libvlc_media_option_trusted);
    if (media && subtitles && track && *track) {
        size_t length=strlen(track)+32;
        char *selection=malloc(length);
        if (selection) {
            snprintf(selection,length,"sub-track-id=%s",track);
            libvlc_media_add_option_flag(media,selection,libvlc_media_option_trusted);
            free(selection);
        }
    }
    free(chain);
    atomic_int stopped_reason=ATOMIC_VAR_INIT(-1);
    const struct libvlc_media_player_cbs callbacks={.version=0,.on_media_stopping=media_stopping};
    libvlc_media_player_t *player=media ? libvlc_media_player_new_from_media(instance,media,&callbacks,&stopped_reason) : NULL;
    int result=5;
    if (diagnostic) { fprintf(stderr,"waveform: player=%d\n",player!=NULL); fflush(stderr); }
    if (player && libvlc_media_player_play(player)==0) {
        for (int step=0; step<(subtitles ? 5900 : 4400); ++step) {
            libvlc_state_t state=libvlc_media_player_get_state(player);
            if (diagnostic && step%100==0) { fprintf(stderr,"waveform: state=%d stopped=%d\n",state,atomic_load(&stopped_reason)); fflush(stderr); }
            if (state==libvlc_Stopped && atomic_load(&stopped_reason)>=0) { result=atomic_load(&stopped_reason); break; }
            if (state==libvlc_Error) { result=6; break; }
#ifdef _WIN32
            Sleep(10);
#else
            usleep(10000);
#endif
        }
    }
    if (player) libvlc_media_player_release(player);
    if (media) libvlc_media_release(media);
    libvlc_release(instance);
    return result;
}
#ifdef _WIN32
int wmain(int argc, wchar_t **argv)
{
    SetDllDirectoryW(L"");
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (argc!=3 && argc!=5) return 2;
    char *arguments[4]={NULL,NULL,NULL,NULL};
    for (int i=0;i<argc-1;++i) {
        int size=WideCharToMultiByte(CP_UTF8,0,argv[i+1],-1,NULL,0,NULL,NULL);
        if (size<=0 || !(arguments[i]=malloc(size))) {
            for (int j=0;j<i;++j) free(arguments[j]);
            return 3;
        }
        WideCharToMultiByte(CP_UTF8,0,argv[i+1],-1,arguments[i],size,NULL,NULL);
    }
    int result=extract(arguments[0],arguments[1],arguments[2],arguments[3]);
    for (int i=0;i<4;++i) free(arguments[i]);
    return result;
}
#else
int main(int argc,char **argv) { return argc==3 || argc==5 ? extract(argv[1],argv[2],argc==5 ? argv[3] : NULL,argc==5 ? argv[4] : NULL) : 2; }
#endif
