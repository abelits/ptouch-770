/*
  ptouch-770-write.c

  Brother PT-H500/P700/E500 printer control utility.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <libudev.h>

/* Column height in bytes */
#define COL_HEIGHT 16

/* Media minimum and maximum width in mm */
#define MEDIA_WIDTH_MIN 4
#define MEDIA_WIDTH_MAX 24

/* Allocate for output buffer */
#define INIT_OUTPUT_SIZE 64

/*
  Read bitmap file into the strip COL_HEIGHT * 8 bytes tall,
  allocate buffer, return allocated buffer size
*/
int read_pbm_file(FILE *f, unsigned char **ptr)
{
  enum
  {
    STATE_PBM_FAIL,
    STATE_PBM_INIT,
    STATE_PBM_TYPE,
    STATE_PBM_DIM_INIT,
    STATE_PBM_DIM_FIRST_NUM,
    STATE_PBM_DIM_FIRST_DONE,
    STATE_PBM_DIM_SECOND_NUM,
    STATE_PBM_DATA_ASCII,
    STATE_PBM_DATA_BIN,
    STATE_PBM_FINISHED
  };

  int c, state = STATE_PBM_INIT;
  int pbm_type = 0,
    pbm_dim_value = 0, pbm_x_dim = 0, pbm_y_dim = 0,
    pbm_x = 0, pbm_y = 0, bitmap_array_size = -1;

  unsigned char *bitmap_array = NULL;

  while((state != STATE_PBM_FINISHED) && ((c = fgetc(f)) >= 0))
    {
      if((c == '#') && (state != STATE_PBM_DATA_BIN))
	{
	  do
	    {
	      c = fgetc(f);
	    }
	  while((c != '\n') && (c >= 0));
	}
      switch(state)
	{
	case STATE_PBM_INIT:
	  if(c == 'P')
	    {
	      state = STATE_PBM_TYPE;
	      pbm_type = 0;
	    }
	  else
	    {
	      state = STATE_PBM_FAIL;
	    }
	  break;

	case STATE_PBM_TYPE:
	  if(c <= ' ')
	    {
	      if((pbm_type != 1) && (pbm_type != 4))
		{
		  state = STATE_PBM_FAIL;
		}
	      else
		{
		  state = STATE_PBM_DIM_INIT;
		  pbm_dim_value = 0;
		}
	    }
	  else
	    {
	      if((c < '0') || (c > '9'))
		state = STATE_PBM_FAIL;
	      else
		{
		  pbm_type *= 10;
		  pbm_type += c - '0';
		}
	    }
	  break;

	case STATE_PBM_DIM_INIT:
	case STATE_PBM_DIM_FIRST_NUM:
	case STATE_PBM_DIM_FIRST_DONE:
	case STATE_PBM_DIM_SECOND_NUM:
	  if(c <= ' ')
	    {
	      if(state == STATE_PBM_DIM_SECOND_NUM)
		{
		  if(pbm_dim_value <= 0)
		    {
		      state = STATE_PBM_FAIL;
		    }
		  else
		    {
		      pbm_y_dim = pbm_dim_value;
		      bitmap_array_size = COL_HEIGHT * pbm_x_dim;
		      bitmap_array = (unsigned char*)malloc(bitmap_array_size);
		      if(!bitmap_array)
			{
			  state = STATE_PBM_FAIL;
			}
		      else
			{
			  memset(bitmap_array, 0, bitmap_array_size);
			  pbm_x = 0;
			  pbm_y = 0;
			  if(pbm_type == 1)
			    {
			      state = STATE_PBM_DATA_ASCII;
			    }
			  else
			    {
			      state = STATE_PBM_DATA_BIN;
			    }
			}
		    }
		}
	      else
		{
		  if(state == STATE_PBM_DIM_FIRST_NUM)
		    {
		      if(pbm_dim_value <= 0)
			{
			  state = STATE_PBM_FAIL;
			}
		      else
			{
			  pbm_x_dim = pbm_dim_value;
			  state = STATE_PBM_DIM_FIRST_DONE;
			  pbm_dim_value = 0;
			}
		    }
		}
	    }
	  else
	    {
	      if((c < '0') || (c > '9'))
		{
		  state = STATE_PBM_FAIL;
		}
	      else
		{
		  if(state == STATE_PBM_DIM_INIT)
		    {
		      state = STATE_PBM_DIM_FIRST_NUM;
		    }
		  else
		    {
		      if(state == STATE_PBM_DIM_FIRST_DONE)
			{
			  state = STATE_PBM_DIM_SECOND_NUM;
			}
		    }
		  pbm_dim_value *= 10;
		  pbm_dim_value += c - '0';
		}
	    }
	  break;
	case STATE_PBM_DATA_ASCII:
	  if((c == '0') || (c == '1'))
	    {
	      if((pbm_x < pbm_x_dim) && (pbm_y < (COL_HEIGHT * 8)))
		{
		  if(c == '1')
		    {
		      bitmap_array[pbm_x * COL_HEIGHT + pbm_y / 8]
			|= 1 << (7 - (pbm_y % 8));
		    }
		}
	      pbm_x++;
	      if(pbm_x >= pbm_x_dim)
		{
		  pbm_x = 0;
		  pbm_y++;
		  if(pbm_y >= pbm_y_dim)
		    {
		      state = STATE_PBM_FINISHED;
		    }
		}
	    }
	  break;
	case STATE_PBM_DATA_BIN:
	  {
	    int i;
	    for(i = 0; i < 8; i++)
	      {
		if(((pbm_x + i) < pbm_x_dim) && (pbm_y < (COL_HEIGHT * 8)))
		  {
		    if(c & (0x80 >> i))
		      {
			bitmap_array[(pbm_x + i) * COL_HEIGHT + pbm_y / 8]
			  |= 1 << (7 - (pbm_y % 8));
		      }
		  }
	      }
	    pbm_x+= 8;
	    if(pbm_x >= pbm_x_dim)
	      {
		pbm_x = 0;
		pbm_y++;
		if(pbm_y >= pbm_y_dim)
		  {
		    state = STATE_PBM_FINISHED;
		  }
	      }
	  }
	  break;
	default:
	  if(bitmap_array)
	    free(bitmap_array);
	  return -1;
	}
      if(state == STATE_PBM_FAIL)
	{
	  if(bitmap_array)
	    free(bitmap_array);
	  return -1;
	}
    }
  *ptr = bitmap_array;
  return bitmap_array_size;
}

/* Write, repear on errors */
int write_persist(int h, void *buffer, size_t size)
{
  unsigned int offset;
  ssize_t l;
  offset = 0;
  while(offset < size)
    {
      l = write(h, ((char*)buffer) + offset, size - offset);
      if( l < 0 && ((errno == EINTR) || (errno == EAGAIN)))
	l = 0;
      if(l < 0)
	return l;
      offset += l;
    }
  return offset;
}

/*
  Write bytes, using write command and a version of PackBits
  RLE for P-Touch PT-H500/P700/E500 label printers.

  It's unclear what exactly are the limits of those printers,
  so only up to COL_HEIGHT bytes can be sent as either same or
  repeating bytes.

  The commands are:
  0x5a : empty column.
  0x47 <size_lsb> <size_msb> <data>: column of compressed data.
  <data> is :
  first byte negative: repeating bytes:     <1 - count> <byte>
  first byte positive: non-repeating bytes: <count - 1> <bytes>
*/
int write_rle(int h, unsigned char *data, size_t size)
{
  unsigned int offset;
  ssize_t l;
  int n, write_size;
  static unsigned char *buffer = NULL;
  static size_t output_size = INIT_OUTPUT_SIZE;
  unsigned char *r_ptr, *r_start_ptr, *w_ptr, *tmpbuf;

  /* Empty column is sent as 0x5a */
  if((size == 16)
     && (!memcmp(data,
		 "\x00\x00\x00\x00\x00\x00\x00\x00"
		 "\x00\x00\x00\x00\x00\x00\x00\x00", 16)))
    {
      if(write_persist(h, "\x5a", 1) == 1)
	return 0;
      else
	return -1;
    }
  
  /* Allocate buffer if it was not already done */
  if(!buffer)
    buffer = (unsigned char*)malloc(output_size);
  if(!buffer)
    return -1;
  
  buffer[0]=0x47;
  w_ptr = buffer + 3;
  r_start_ptr = data;
  while((r_start_ptr - data) < size)
    {
      r_ptr = r_start_ptr + 1;
      
      /* At least two bytes should be available */
      if((output_size - (w_ptr - buffer)) < 2)
	{
	  tmpbuf = realloc(buffer, w_ptr - buffer + 2);
	  if(!tmpbuf)
	    return -1;
	  output_size = w_ptr - buffer + 2;
	  w_ptr = tmpbuf + (w_ptr - buffer);
	  buffer = tmpbuf;
	}
      
      /*
	Substring of up to COL_HEIGHT bytes with the same value
	can be replaced with a two-byte sequence
      */
      while(((r_ptr - data) < size)
	    && ((r_ptr - r_start_ptr) < COL_HEIGHT)
	    && (*r_ptr == *r_start_ptr))
	r_ptr++;
      n = r_ptr - r_start_ptr;
      if(n > 2)
	{
	  /* Repeating bytes, write encoded as two bytes */
	  *w_ptr++ = (unsigned char)(1 - (signed char)n);
	  *w_ptr++ = *r_start_ptr;
	}
      else
	{
	  /* Substring of up to COL_HEIGHT bytes that do not repeat */
	  while(((r_ptr - data) < size)
		&& ((r_ptr - r_start_ptr) < COL_HEIGHT)
		&& (*r_ptr != r_ptr[1]))
	    r_ptr++;
	  n = r_ptr - r_start_ptr;
	  *w_ptr++ = (unsigned char)n - 1;
	  
	  if(n > 0)
	    {
	      /* At least n bytes should be available */
	      if((output_size - (w_ptr - buffer)) < n)
		{
		  tmpbuf = realloc(buffer, w_ptr - buffer + n);
		  if(!tmpbuf)
		    return -1;
		  output_size = w_ptr - buffer + n;
		  w_ptr = tmpbuf + (w_ptr - buffer);
		  buffer = tmpbuf;
		}
	      /* Copy data */
	      while(r_start_ptr < r_ptr)
		*w_ptr++ = *r_start_ptr++;
	    }
	}
      r_start_ptr = r_ptr;
    }
  
  offset = 0;
  write_size = w_ptr - buffer;
  buffer[1] = (unsigned char)((write_size - 3) & 0xff);
  buffer[2] = (unsigned char)(((write_size - 3) >> 8) & 0xff);
  while(offset < write_size)
    {
      l = write(h, buffer + offset, write_size - offset);
      if( l < 0 && ((errno == EINTR) || (errno == EAGAIN)))
	l = 0;
      if(l < 0)
	return -2;
      offset += l;
    }
  return 0;
}

/*
  Get printer status.
  If query is sent, wait for response, otherwise
  return error if response is not available.
 */
int get_printer_status(int h, int query, int *media_width)
{
  int offset, l;
  unsigned char response[32];
  if(query)
    {
      if(write_persist(h, "\x1b\x69\x53", 3) != 3)
	{
	  return -1;
	}
    }

  offset = 0;
  do
    {
      l = read(h, &response + offset, 32 - offset);
      
      if(l > 0 )
	{
	  if(l > (32 - offset))
	    l = 32 - offset;
	  offset += l;
	}
      else
	{
	  if(query)
	    {
	      if((l < 0) && (errno != EINTR) && (errno != EAGAIN))
		return -1;
	    }
	  else
	    {
	      if(offset == 0)
		return -1;
	    }
	}
    }
  while(offset < 32);

  if(media_width)
    {
      *media_width = response[10];
    }
  /*
  int i;
  printf("Returned data:");
  for(i = 0; i < 32; i++)
    printf(" %02x", response[i]);
  printf("\n");
  */
  return 0;
}

/* main */
int main(int argc, char **argv)
{
  int h, i, l;
  unsigned char *data_buffer;
  FILE *f;

  /* Open bitmap file, don't do anything with it yet */
  if(argc < 2)
    {
      fprintf(stderr, "Usage: %s <file.pbm>\n", argv[0]);
      return 1;
    }

  f = fopen(argv[1], "r");
  if(!f)
    {
      fprintf(stderr, "Can't open bitmap file\n");
      return 1;
    }

  /* Find Brother PT-H500/P700/E500 device */
  int udev_failed = 0;
  struct udev *udev;
  struct udev_enumerate *udev_enum = NULL;
  struct udev_list_entry *udev_devices, *udev_entry;
  struct udev_device *udev_dev, *udev_dev_usb;
  const char *syspath, *vendor, *product, *devpath_found;
  char *devpath = NULL;
  
  udev = udev_new();
  
  if(udev)
    {
      udev_enum = udev_enumerate_new(udev);
      if(!udev_enum)
	udev_failed = 1;
    }
  else
    udev_failed = 1;
  
  if(!udev_failed
     && udev_enumerate_add_match_subsystem(udev_enum, "usbmisc") < 0)
    udev_failed = 1;

  if(!udev_failed
     && udev_enumerate_scan_devices(udev_enum) < 0)
    udev_failed = 1;
  
  if(!udev_failed
     && ((udev_devices = udev_enumerate_get_list_entry(udev_enum)) < 0))
    udev_failed = 1;

  if(!udev_failed)
    {
      udev_list_entry_foreach(udev_entry, udev_devices)
	{
	  if(devpath == NULL)
	    {
	      syspath = udev_list_entry_get_name(udev_entry);
	      if(syspath)
		udev_dev = udev_device_new_from_syspath(udev, syspath);
	      else
		udev_dev = NULL;
	      if(udev_dev)
		{
		  udev_dev_usb = 
		    udev_device_get_parent_with_subsystem_devtype(udev_dev,
								  "usb",
								  "usb_device");
		}
	      else
		udev_dev_usb = NULL;

	      if(udev_dev_usb)
		{
		  vendor =
		    udev_device_get_sysattr_value(udev_dev_usb,"idVendor");
		  product = 
		    udev_device_get_sysattr_value(udev_dev_usb,"idProduct");
		  if(vendor && product
		     && !strcasecmp(vendor,"04f9") /* Brother */
		     && (!strcasecmp(product,"205e") /* PT-H500 */
			 || !strcasecmp(product,"205f") /* PT-E500 */
			 || !strcasecmp(product,"2061") /* PT-P700 */))
		    {
		      devpath_found = udev_device_get_devnode(udev_dev);
		      if(devpath_found)
			devpath = strdup(devpath_found);
		    }
		  udev_device_unref(udev_dev_usb);
		}
	    }
	}
    }
  
  if(udev_enum)
    udev_enumerate_unref(udev_enum);

  if(udev)
    udev_unref(udev);
  
  if(udev_failed)
    {
      fprintf(stderr, "Can't initialize udev\n");
      return 1;
    }
  
  if(!devpath)
    {
      fprintf(stderr, "Brother PT-H500/P700/E500 printer not found\n");
      return 1;
    }

  /* Open device */
  h = open(devpath, O_RDWR);
  free(devpath);
  if(h < 0)
    {
      perror("Can't open printer");
      return 1;
    }
  
  /* Initialization */
  unsigned char cmd_buffer[102];
  memset(cmd_buffer, 0, 100);
  cmd_buffer[100] = 0x1b;
  cmd_buffer[101] = 0x40;
  if(write_persist(h, cmd_buffer, 102) != 102)
    {
      return 1;
    }

  int media_width = MEDIA_WIDTH_MAX;
  
  /* Get status */
  if(get_printer_status(h, 1, &media_width))
    {
      return 1;
    }

  if((media_width < MEDIA_WIDTH_MIN) || (media_width > MEDIA_WIDTH_MAX))
    {
      fprintf(stderr, "Replace label tape cartridge\n");
      return 1;
    }

  l = read_pbm_file(f, &data_buffer);
  fclose(f);

  if(l < 0)
    {
      fprintf(stderr, "Can't read bitmap file\n");
      return 1;
    }

  
  /* Dynamic command mode setting: raster */
  if(write_persist(h, "\x1b\x69\x61\x01", 4) != 4)
    {
      return 1;
    }
  
  /*
    Print information (parameters) command:
    1: flags: printer recovery on, set media width,
    2: media type: unused
    3: media width: value from the printer
    4: media length: unused
    5-8: number of raster columns, lsb first
    9: starting page
    10: unused
  */
  memcpy(cmd_buffer,
	 "\x1b\x69\x7a\x84", 4);                    /* 1 */
  cmd_buffer[4] = 0;                                /* 2 */
  cmd_buffer[5] = media_width;                      /* 3 */
  cmd_buffer[6] = 0;                                /* 4 */
  cmd_buffer[7] = (l / COL_HEIGHT) & 0xff;          /* 5 */
  cmd_buffer[8] = ((l / COL_HEIGHT) >> 8) & 0xff;   /* 6 */
  cmd_buffer[9] = ((l / COL_HEIGHT) >> 16) & 0xff;  /* 7 */
  cmd_buffer[10] = ((l / COL_HEIGHT) >> 24) & 0xff; /* 8 */
  cmd_buffer[11] = 0;                               /* 9 */
  cmd_buffer[12] = 0;                               /* 10 */

  /* Auto cut before output */
  memcpy(cmd_buffer + 13, "\x1b\x69\x4d\x40", 4); 

  /* 
     This was a command for number of pages before cut,
     apparently not used in this model.
  */
  /*
  memcpy(cmd_buffer + 17, "\x1b\x69\x41\x01", 4);
  */

  /* No chain printing (cut after the label) */
  memcpy(cmd_buffer + 21, "\x1b\x69\x4b\x08", 4); 
#if 0
  /* Margins 2mm */
  memcpy(cmd_buffer + 25, "\x1b\x69\x64\x0e\x00", 5); 
#else
  /* Margins 32 pixels */
  memcpy(cmd_buffer + 25, "\x1b\x69\x64\x20\x00", 5); 
#endif
  /* Compression on */
  memcpy(cmd_buffer + 30, "\x4d\x02", 2);
  if(write_persist(h, cmd_buffer, 32) != 32)
    {
      return 1;
    }
  for(i = 0; i < l; i += 16)
    {
      write_rle(h, data_buffer + i, 16);
      get_printer_status(h, 0, NULL);
    }
  free(data_buffer);
  /* Print */
  if(write_persist(h, "\x1a", 1) != 1)
    {
      return 1;
    }
  get_printer_status(h, 0, NULL);
  get_printer_status(h, 1, NULL);
  close(h);
  return 0;
}
