#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>

void write_bmp_file(unsigned char* image_data, int width, int height, const char* filename) {
    int row_padding = (4 - (width * 3) % 4) % 4;
    int data_size = width * height * 3 + height * row_padding;
    int file_size = 54 + data_size;
    char width_bmp[4] = {(char)width, (char)(width >> 8),(char)(width >> 16),(char)(width >> 16+8)};
    char height_bmp[4] = {(char)height, (char)(height >> 8),(char)(height >> 16),(char)(height >> 16+8)};

    unsigned char* bmp_data = (unsigned char*) malloc(data_size);
    /*
        In the header segment all the image structure and how the format is used exist in 54 byte
        NOTICE: As width & height would be more than 254 they can't be written directly so casting would be needed and shifting for each byte so that no data is lost
        i.e.:
            int width = 640     (This is an int so 4 bytes = 00000000 00000000 00000010 10000000)
            char width can't be 640 so 
            char width[4]
            where width[0] = 0b10000000 and width[1] = 01000000
                By casting directly the first least segnficant byte will be taken and the rest will be lost
    */
    unsigned char bmp_header[54] = {
        'B', 'M',       // Signature
        file_size, 0, 0, 0,  // File size
        0, 0, 0, 0,          // Reserved
        54, 0, 0, 0,         // Offset to pixel data
        40, 0, 0, 0,         // Header size
        width_bmp[0], width_bmp[1], width_bmp[2], width_bmp[3],      // Image width
        height_bmp[0],height_bmp[1],height_bmp[2],height_bmp[3],     // Image height
        1, 0,               // Number of color planes
        24, 0,              // Bits per pixel
        0, 0, 0, 0,         // Compression method
        data_size, 0, 0, 0,  // Image size
        0, 0, 0, 0,         // Horizontal resolution
        0, 0, 0, 0,         // Vertical resolution
        0, 0, 0, 0,         // Number of colors in color palette
        0, 0, 0, 0          // Number of important colors
    };

    // Convert RGB to BGR
    /*
        in BMP format pixels are reversed and take a bottom-up order
        From jpeg to bmp I started with the first pixel in the last row and kept looping for the width of the last row
        the width in the bmp format or in the row data would be the width of the resolution multiplied by 3 for the RGB
            JPEG                    bmp
        .   .   .   .           *   .   .   .
        .   .   .   .           .   .   .   .
        .   .   .   .           .   .   .   .
        .   .   .   .           .   .   .   .
        *   .   .   .           .   .   .   .

        The image data would be in one dimension as files' data must be one dimensional

            JPEG                    bmp
        . . . . * . . .         * . . . . . . . 
    */
    for (int i = width * height * 3 - width*3, j = 0; i > 0; i -= width*3, j += width*3) {
        for(int k=0;k<width*3;k+=3){
            bmp_data[j+k] = image_data[(i+k) + 2];
            bmp_data[(j+k) + 1] = image_data[(i+k) + 1];
            bmp_data[(j+k) + 2] = image_data[i+k];
        }
        
    }

    FILE* bmp_file = fopen(filename, "wb");
    if (bmp_file == NULL) {
        fprintf(stderr, "Failed to create BMP file.\n");
        free(bmp_data);
        return;
    }

    fwrite(bmp_header, sizeof(unsigned char), 54, bmp_file);
    fwrite(bmp_data, sizeof(unsigned char), data_size, bmp_file);

    fclose(bmp_file);
    free(bmp_data);
}

int main() {
    const char* jpeg_filename = "0.jpeg";
    const char* bmp_filename = "0.bmp";

    // Read the JPEG file
    FILE* jpeg_file = fopen(jpeg_filename, "rb");
    if (jpeg_file == NULL) {
        fprintf(stderr, "Failed to open JPEG file.\n");
        return 1;
    }

    // Decompress the jpeg file and get the orginal data
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, jpeg_file);
    jpeg_read_header(&cinfo, TRUE);

    jpeg_start_decompress(&cinfo);

    int width = cinfo.output_width;
    int height = cinfo.output_height;
    int num_channels = cinfo.output_components;

    unsigned char* image_data = (unsigned char*) malloc(width * height * num_channels);
    unsigned char* row_pointer[1];

    while (cinfo.output_scanline < cinfo.output_height) {
        row_pointer[0] = image_data + (cinfo.output_scanline) * width * num_channels;
        jpeg_read_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(jpeg_file);

    // Write the BMP file
    printf("%d %d %d\n",width,height,num_channels);
    write_bmp_file(image_data, width, height, bmp_filename);

    free(image_data);

    printf("Conversion complete.\n");

    return 0;
}