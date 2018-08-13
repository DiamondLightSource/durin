!
! This is free and unencumbered software released into the public domain.!
! Anyone is free to copy, modify, publish, use, compile, sell, or distribute this software, 
! either in source code form or as a compiled binary, for any purpose, commercial or non-commercial,
! and by any means.
!
! In jurisdictions that recognize copyright laws, the author or authors of this software dedicate 
! any and all copyright interest in the software to the public domain. We make this dedication for
! the benefit of the public at large and to the detriment of our heirs and successors. We intend
! this dedication to be an overt act of relinquishment in perpetuity of all present and future 
! rights to this software under copyright law.
!
! THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
! NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
! IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
! ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR 
! THE USE OR OTHER DEALINGS IN THE SOFTWARE.
!
! For more information, please refer to <http://unlicense.org/>
!
!
! vittorio.boccone@dectris.com
! Dectris Ltd., Taefernweg 1, 5405 Baden-Daettwil, Switzerland.
!
! (proof_of_concept)
!
! Interoperability with C in Fortran 2003
!
! Wrap up module to abstract the interface from 
! http://cims.nyu.edu/~donev/Fortran/DLL/DLL.Forum.txt
!
module iso_c_utilities
   use iso_c_binding ! intrinsic module

   character(c_char), dimension(1), save, target, private :: dummy_string="?"
   
contains   
   
   function c_f_string(cptr) result(fptr)
      ! convert a null-terminated c string into a fortran character array pointer
      type(c_ptr), intent(in) :: cptr ! the c address
      character(kind=c_char), dimension(:), pointer :: fptr
      
      interface ! strlen is a standard C function from <string.h>
         function strlen(string) result(len) bind(C,name="strlen")
            use iso_c_binding
            type(c_ptr), value :: string ! a C pointer
         end function
      end interface   
      
      if(c_associated(cptr)) then
         call c_f_pointer(fptr=fptr, cptr=cptr, shape=[strlen(cptr)])
      else
         ! to avoid segfaults, associate fptr with a dummy target:
         fptr=>dummy_string
      end if
            
   end function

end module iso_c_utilities

!
! Interoperability with C in Fortran 2003
!
! Wrap up module to abstract the interface from 
! http://cims.nyu.edu/~donev/Fortran/DLL/DLL.Forum.txt
!
module dlfcn
   use iso_c_binding
   use iso_c_utilities
   implicit none
   private

   public :: dlopen, dlsym, dlclose, dlerror ! dl api
   
   ! valid modes for mode in dlopen:
   integer(c_int), parameter, public :: rtld_lazy=1, rtld_now=2, rtld_global=256, rtld_local=0
      ! obtained from the output of the previously listed c program 
         
   interface ! all we need is interfaces for the prototypes in <dlfcn.h>
      function dlopen(file,mode) result(handle) bind(C,name="dlopen")
         ! void *dlopen(const char *file, int mode);
         use iso_c_binding
         character(c_char), dimension(*), intent(in) :: file
            ! c strings should be declared as character arrays
         integer(c_int), value :: mode
         type(c_ptr) :: handle
      end function
      function dlsym(handle,name) result(funptr) bind(C,name="dlsym")
         ! void *dlsym(void *handle, const char *name);
         use iso_c_binding
         type(c_ptr), value :: handle
         character(c_char), dimension(*), intent(in) :: name
         type(c_funptr) :: funptr ! a function pointer
      end function
      function dlclose(handle) result(status) bind(C,name="dlclose")
         ! int dlclose(void *handle);
         use iso_c_binding
         type(c_ptr), value :: handle
         integer(c_int) :: status
      end function
      function dlerror() result(error) bind(C,name="dlerror")
         ! char *dlerror(void);
         use iso_c_binding
         type(c_ptr) :: error
      end function         
   end interface
      
 end module dlfcn

!
! Generic handle for share-object like structures
!
! Wrap up module to abstract the interface from 
! http://cims.nyu.edu/~donev/Fortran/DLL/DLL.Forum.txt
!
module generic_data_plugin
  use iso_c_binding
  implicit none

  character(kind=c_char,len=1024) :: dll_filename
  character(kind=c_char,len=1024) :: image_data_filename
  integer(c_int)                  :: status
  type(c_ptr)                     :: handle=c_null_ptr
  INTEGER :: nx,ny,firstqm=0,lastqm=0   ! global variables that do not change    
! firstqm, lastq     mark ? characters in NAME_TEMPLATE that get replaced by an image number
  CHARACTER(len=:), allocatable :: library ! global variable that does not change 
  LOGICAL :: is_open=.FALSE.           ! set .TRUE. if library successfully opened

  !public                          :: generic_open !, generic_header, generic_data, generic_clone

  !
  ! Abstract interfaces for C mapped functions
  !
  !
  ! get_header -> dll_get_header 
  abstract interface

     subroutine plugin_open(filename, info_array, error_flag) bind(C)
       use iso_c_binding
       integer(c_int)                  :: error_flag
       character(kind=c_char)          :: filename(*)
       integer(c_int), dimension(1024) :: info_array


     end subroutine plugin_open

     subroutine plugin_close(error_flag) bind(C)
       use iso_c_binding
       integer (c_int)          :: error_flag

     end subroutine plugin_close

     subroutine plugin_get_header(nx, ny, nbyte, qx, qy, number_of_frames, info_array, error_flag) bind(C)
       use iso_c_binding
       integer(c_int)                  :: nx, ny, nbyte, number_of_frames       
       real(c_float)                   :: qx, qy
       integer(c_int)                  :: error_flag
       integer(c_int), dimension(1024) :: info_array
     end subroutine plugin_get_header

     subroutine plugin_get_data(frame_number, nx, ny, data_array, info_array, error_flag) bind(C)
       use iso_c_binding
       integer(c_int)                   :: nx, ny, frame_number
       integer(c_int)                   :: error_flag
       integer(c_int), dimension(nx:ny) :: data_array
       integer(c_int), dimension(1024)  :: info_array
     end subroutine plugin_get_data
  end interface

  ! dynamically-linked procedures
  procedure(plugin_open),  pointer :: dll_plugin_open
  procedure(plugin_get_header), pointer :: dll_plugin_get_header 
  procedure(plugin_get_data),   pointer :: dll_plugin_get_data   
  procedure(plugin_close), pointer :: dll_plugin_close
   



contains


  ! 
  ! Open the shared-object 
  subroutine generic_open(library, template_name, info_array, error_flag)    ! Requirements:
    !  'LIBRARY'                      input  (including path, otherwise using LD_LIBRARY_PATH)
    !  'TEMPLATE_NAME'                input  (the resource in image data masterfile)
    !  'INFO' (integer array)         input  Array of (1024) integers:
    !                                          INFO(1)    = Consumer ID (1:XDS)
    !                                          INFO(2)    = Version Number of the Consumer software
    !                                          INFO(3:8)  = Unused
    !                                          INFO(9:40) = 1024bit signature of the consumer software
    !                                          INFO(>41)  = Unused
    !  'INFO' (integer array)         output Array of (1024) integers:
    !                                          INFO(1)    = Vendor ID (1:Dectris)
    !                                          INFO(2)    = Major Version number of the library
    !                                          INFO(3)    = Minor Version number of the library
    !                                          INFO(4)    = Parch Version number of the library
    !                                          INFO(5)    = Linux timestamp of library creation
    !                                          INFO(6:8)  = Unused
    !                                          INFO(9:40) = 1024bit signature of the library
    !                                          INFO(>41)  = Unused
    !  'ERROR_FLAG'                   output Return values
    !                                         0 Success
    !                                        -1 Handle already exists
    !                                        -2 Cannot open Library
    !                                        -3 Function not found in library
    !                                        -4 Master file cannot be opened (coming from C function)
    !                                        -10 Consumer identity not supported (coming from C function)
    !                                        -11 Consumer identity could not be verified (coming from C function)
    !                                        -12 Consumer software version not supported (coming from C function)

    use iso_c_binding
    use iso_c_utilities
    use dlfcn
    implicit none    

    character(len=:), allocatable      :: library, template_name
    integer(c_int)                     :: error_flag
    integer(c_int), dimension(1024)    :: info_array
    type(c_funptr)                     :: fun_plugin_open_ptr   = c_null_funptr
    type(c_funptr)                     :: fun_plugin_close_ptr  = c_null_funptr
    type(c_funptr)                     :: fun_plugin_get_header_ptr  = c_null_funptr
    type(c_funptr)                     :: fun_plugin_get_data_ptr    = c_null_funptr
    integer(c_int)                     :: external_error_flag
    logical                            :: loading_error_flag     = .false.

    error_flag=0

    write(6,*) "[generic_data_plugin] - INFO - generic_open"
    write(6,*) "      + library          = <", library,      ">"
    write(6,*) "      + template_name    = <", template_name, ">"

    if ( c_associated(handle) ) then
       write(6,*) "[generic_data_plugin] - ERROR - 'handle' not null"
       error_flag = -1
       return
    endif

    dll_filename=library
    error_flag = 0 
    write(6,*)  "      + dll_filename     = <", trim(dll_filename)//C_NULL_CHAR, ">"
 
    image_data_filename=trim(template_name)//C_NULL_CHAR
    error_flag = 0 
    write(6,*)  "      + image_data_filename   = <", trim(image_data_filename)//C_NULL_CHAR, ">"

    !
    ! Open the DL:
    ! The use of IOR is not really proper...wait till Fortran 2008  
    handle=dlopen(trim(dll_filename)//C_NULL_CHAR, IOR(RTLD_NOW, RTLD_GLOBAL))

    !
    ! Check if can use handle
    if(.not.c_associated(handle)) then
       write(6,*) "[generic_data_plugin] - ERROR - Cannot open Handle: ", c_f_string(dlerror())
       error_flag = -2
       return
    end if
    

    !
    ! Find the subroutines in the DL:
    fun_plugin_get_data_ptr   = DLSym(handle,"plugin_get_data")
    if(.not.c_associated(fun_plugin_get_data_ptr))  then
       write(6,*) "[generic_data_plugin] - ERROR in DLSym(handle,'plugin_get_data'): ", c_f_string(dlerror())
       loading_error_flag = .true.
    else
       call c_f_procpointer(cptr=fun_plugin_get_data_ptr,   fptr=dll_plugin_get_data)
    endif
    !
    fun_plugin_get_header_ptr = DLSym(handle,"plugin_get_header")
    if(.not.c_associated(fun_plugin_get_header_ptr))  then
       write(6,*) "[generic_data_plugin] - ERROR in DLSym(handle,'plugin_get_header'): ",c_f_string(dlerror())
       loading_error_flag = .true.
    else
       call c_f_procpointer(cptr=fun_plugin_get_header_ptr, fptr=dll_plugin_get_header)
    endif
    !
    fun_plugin_open_ptr   = DLSym(handle,"plugin_open")
    if(.not.c_associated(fun_plugin_open_ptr))  then
       write(6,*) "[generic_data_plugin] - ERROR in DLSym(handle,'plugin_open'): ", c_f_string(dlerror())
       loading_error_flag = .true.
    else
       call c_f_procpointer(cptr=fun_plugin_open_ptr,   fptr=dll_plugin_open)
    endif
    
    fun_plugin_close_ptr = DLSym(handle,"plugin_close")
    if(.not.c_associated(fun_plugin_close_ptr)) then
       write(6,*) "[generic_data_plugin] - ERROR in DLSym(handle,'plugin_close'): ", c_f_string(dlerror())
       loading_error_flag = .true.
    else
       call c_f_procpointer(cptr=fun_plugin_close_ptr, fptr=dll_plugin_close)
    endif


    if (loading_error_flag) then
       write(6,*) "[generic_data_plugin] - ERROR - Cannot map function(s) from the dll"
       error_flag = -3
    else   
       call dll_plugin_open(image_data_filename, info_array, external_error_flag)
       error_flag = external_error_flag
    endif
    IF (error_flag == 0) is_open=.TRUE.
    return

  end subroutine generic_open

  !
  ! Get the header
  subroutine generic_get_header(nx, ny, nbyte, qx, qy, number_of_frames, info_array, error_flag)
    ! Requirements:
    !  'NX' (integer)                  output  Number of pixels along X 
    !  'NY' (integer)                  output  Number of pixels along Y
    !  'NBYTE' (integer)               output  Number of bytes in the image... X*Y*DEPTH
    !  'QX' (4*REAL)                   output  Pixel size
    !  'QY' (4*REAL)                   output  Pixel size
    !  'NUMBER_OF_FRAMES' (integer)    output  Number of frames for the full datase. So far unused
    !  'INFO' (integer array)           input  Array of (1024) integers:
    !                                          INFO(>1)     = Unused
    !  'INFO' (integer array)          output  Array of (1024) integers:
    !                                           INFO(1)       = Vendor ID (1:Dectris)
    !                                           INFO(2)       = Major Version number of the library
    !                                           INFO(3)       = Minor Version number of the library
    !                                           INFO(4)       = Patch Version number of the library
    !                                           INFO(5)       = Linux timestamp of library creation
    !                                           INFO(6:64)    = Reserved
    !                                           INFO(65:1024) = Dataset parameters
    !  'ERROR_FLAG'                    output  Return values
    !                                            0      Success
    !                                           -1      Cannot open library
    !                                           -2      Cannot read header (will come from C function)
    !                                           -4      Cannot read dataset informations (will come from C function)
    !                                           -10     Error in the determination of the Dataset parameters (will come from C function)
    !
    use iso_c_binding
    use iso_c_utilities
    use dlfcn
    implicit none    
    
    integer(c_int)                   :: nx, ny, nbyte, number_of_frames
    real(c_float)                    :: qx, qy
    integer(c_int)                   :: error_flag
    integer(c_int)                   :: external_error_flag
    integer(c_int), dimension(1024)  :: info_array
    error_flag=0

    write(6,*) "[generic_data_plugin] - INFO - generic_get_header"
    
    ! Check if can use handle
    if(.not.c_associated(handle)) then
       write(6,*) "[generic_data_plugin] - ERROR - Cannot open Handle"
       write(6,*) "                        ", c_f_string(dlerror())
       error_flag = -1
       return
    end if
 
    ! finally, invoke the dynamically-linked subroutine:
    call dll_plugin_get_header(nx, ny, nbyte, qx, qy, number_of_frames, info_array, external_error_flag)
    return 
  end subroutine generic_get_header


  ! 
  ! Dynamically map function and execute it 
  subroutine generic_get_data(frame_number, nx, ny, data_array, info_array, error_flag)
    ! Requirements:
    !  'FRAME_NUMBER' (integer)        input  Number of frames for the full datase. So far unused
    !  'NX' (integer)                  input  Number of pixels along X 
    !  'NY' (integer)                  input  Number of pixels along Y
    !  'DATA_ARRAY' (integer array)   output  1D array containing pixel data with lenght = NX*NY
    !  'INFO' (integer array)         output Array of (1024) integers:
    !                                          INFO(1)     = Vendor ID (1:Dectris)
    !                                          INFO(2)     = Major Version number of the library
    !                                          INFO(3)     = Minor Version number of the library
    !                                          INFO(4)     = Parch Version number of the library
    !                                          INFO(5)     = Linux timestamp of library creation
    !                                          INFO(6:8)   = Unused
    !                                          INFO(9:40)  = 1024bit verification key
    !                                          INFO(41:44) = Image MD5 Checksum 
    !                                          INFO()  = Unused
    !  'ERROR_FLAG' (integer)         output  Provides error state condition
    !                                           0 Success
    !                                          -1 Cannot open library 
    !                                          -2 Cannot open frame (will come from C function)
    !                                          -3 Datatype not supported (will come from C function)
    !                                          -4 Cannot read dataset informations (will come from C function)
    !                                         -10 MD5 Checksum Error 
    !                                         -11 Verification key error
    !  
    use iso_c_binding
    use iso_c_utilities
    use dlfcn
    implicit none    

    integer(c_int)                    :: nx, ny, frame_number
    integer(c_int)                    :: error_flag
    integer(c_int), dimension(1024)   :: info_array
    integer(c_int), dimension (nx*ny) :: data_array


    error_flag=0
    call dll_plugin_get_data(frame_number, nx, ny, data_array, info_array, error_flag)
   
  end subroutine generic_get_data

  ! Close the shared-object 
  ! 
  subroutine generic_close(error_flag)
    ! Requirements:
    !      'ERROR_FLAG' (integer)     output  Return values:
    !                                           0 Success
    !                                          -1 Error closing Masterfile
    !                                          -2 Error closing Shared-object

    use iso_c_binding
    use iso_c_utilities
    use dlfcn
    implicit none    

    integer(c_int) :: error_flag
    integer(c_int) :: external_error_flag

    IF (.NOT.is_open) RETURN
    ! Check if can use handle
    if(.not.c_associated(handle)) then
       write(6,*) "[generic_data_plugin] - ERROR - Cannot open Handle"
       write(6,*) "                        ", c_f_string(dlerror())
       error_flag = -1
       return
    end if

    write(6,*) "[generic_data_plugin] - INFO - 'call generic_close()'"
    
    call dll_plugin_close(external_error_flag)
    error_flag = external_error_flag

    ! now close the dl:
    status=dlclose(handle)
    if(status/=0) then
       write(6,*) "[generic_data_plugin] - ERROR - Cannot open Handle"
       write(6,*) "                        ", c_f_string(dlerror())
       error_flag = -2
    else
       error_flag = 0
    end if

    return 
  end subroutine generic_close

end module generic_data_plugin
