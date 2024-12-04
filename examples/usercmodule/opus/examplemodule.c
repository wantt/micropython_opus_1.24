/*
ref:
    http://124.223.103.23/index.php/archives/esp32-opus.html
    https://chatgpt.com/share/6709e6f0-8bc4-8003-a750-59179595c871
    micropython 形参设置: https://chatgpt.com/c/670d0b2e-cb20-8003-ad0f-6ac80f9a682c
*/
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/misc.h"
#include "py/objarray.h"

#include <stdio.h>
#include <stdbool.h>
#include <opus.h>

#define MAX_PACKET_SIZE 1276

void check_error(int error) {
    if (error != OPUS_OK) {
        fprintf(stderr, "Opus error: %s\n", opus_strerror(error));
        exit(EXIT_FAILURE);
    }
}

int decoder(unsigned char encoded_data[], int nb_bytes, short out_pcm[],int frame_size,int32_t sample_rate,bool is_final){
    static OpusDecoder *decoder_handle = NULL;
    if(!decoder_handle){
        int err = 0;
        int channels = 1;
        decoder_handle = opus_decoder_create(sample_rate, channels, &err);
        check_error(err);
    }
    if(is_final) return opus_decoder_ctl(decoder_handle, OPUS_RESET_STATE);
    if(nb_bytes <=0) return  2*opus_decode(decoder_handle, NULL, 0, out_pcm, frame_size, 1);
    return 2*opus_decode(decoder_handle, encoded_data, nb_bytes, out_pcm, frame_size, 0);
}

int32_t encoder(short in_pcm[],int frame_len,unsigned char encoded_data[], int32_t sample_rate,int32_t bitrate ,bool is_final){
    static OpusEncoder *encoder_handle = NULL;
    if(!encoder_handle){
        int channels = 1; // 1
        int complexity = 0; // 0
        int frame_duration = OPUS_FRAMESIZE_10_MS; // 5003= 10ms 5004=20ms  5005=40ms
        int application_type = OPUS_APPLICATION_AUDIO;  // 2049=audio, 2048=voip
        int err = 0;
        encoder_handle = opus_encoder_create(sample_rate, channels, application_type, &err);
        check_error(err);
        opus_encoder_ctl(encoder_handle, OPUS_SET_BITRATE(bitrate));
        // opus_encoder_ctl(encoder_handle, OPUS_SET_VBR(1));
        opus_encoder_ctl(encoder_handle, OPUS_SET_COMPLEXITY(complexity));
        opus_encoder_ctl(encoder_handle, OPUS_SET_EXPERT_FRAME_DURATION(frame_duration));
        // opus_encoder_ctl(encoder_handle, OPUS_SET_FORCE_MODE(MODE_CELT_ONLY));
	//opus_encoder_ctl(encoder_handle, OPUS_SET_INBAND_FEC(1));  // 启用 Inband FEC
        // 设置丢包率为10%
        //opus_encoder_ctl(encoder_handle, OPUS_SET_PACKET_LOSS_PERC(10));   
    }
    if(is_final) return  (int32_t) opus_encoder_ctl(encoder_handle, OPUS_RESET_STATE);
    return opus_encode(encoder_handle, in_pcm, frame_len, encoded_data, MAX_PACKET_SIZE);
}



mp_obj_t uencode(size_t n_args, const mp_obj_t *args){
    mp_obj_array_t *mp_in_array = MP_OBJ_TO_PTR(args[0]); // in_pcm
    int16_t *in_pcm = (int16_t*) mp_in_array->items;
    int frame_len = (int)mp_obj_get_int(args[1]);
    mp_obj_array_t *mp_encoded_array = MP_OBJ_TO_PTR(args[2]);
    unsigned char *encoded_data = (unsigned char*) mp_encoded_array->items;
    int32_t sample_rate = (int32_t)mp_obj_get_int(args[3]);
    int32_t bitrate = (int32_t)mp_obj_get_int(args[4]);
    bool is_final = mp_obj_is_true(args[5]);
    return MP_OBJ_FROM_PTR(mp_obj_new_int( encoder(in_pcm, frame_len, encoded_data,sample_rate, bitrate,is_final)));
}

mp_obj_t udecode(size_t n_args, const mp_obj_t *args){
    mp_obj_array_t *mp_encode_array = MP_OBJ_TO_PTR(args[0]); 
    unsigned char * encoded_data = (unsigned char *)mp_encode_array->items;
    int nb_bytes = (int)mp_obj_get_int(args[1]);
    mp_obj_array_t *mp_in_array = MP_OBJ_TO_PTR(args[2]); // encoded_data
    short *out_pcm = (short*) mp_in_array->items;
    int frame_size = (int)mp_obj_get_int(args[3]);
    int32_t sample_rate = (int32_t)mp_obj_get_int(args[4]);
    bool is_final = mp_obj_is_true(args[5]);
    return MP_OBJ_FROM_PTR( mp_obj_new_int(   decoder( encoded_data, nb_bytes, out_pcm, frame_size,sample_rate,is_final)   ));
}

static MP_DEFINE_CONST_FUN_OBJ_VAR(example_decode, 0, udecode);
static MP_DEFINE_CONST_FUN_OBJ_VAR(example_encode, 0, uencode);
static const mp_rom_map_elem_t example_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_opus) },
    { MP_ROM_QSTR(MP_QSTR_decode), MP_ROM_PTR(&example_decode) },
    { MP_ROM_QSTR(MP_QSTR_encode), MP_ROM_PTR(&example_encode) },
};
static MP_DEFINE_CONST_DICT(example_module_globals, example_module_globals_table);

// Define module object.
const mp_obj_module_t example_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&example_module_globals,
};

// Register the module to make it available in Python.
MP_REGISTER_MODULE(MP_QSTR_opus, example_user_cmodule);
