! Example test program for existing external library
! This should be saved in a file called test_generic_host.f90
! Kay Diederichs 4/2017
!
! compile with 
! ifort -qopenmp generic_data_plugin.f90 test_generic_host.f90 -o test_generic_host
! or
! gfortran -O -fopenmp -ldl generic_data_plugin.f90 test_generic.f90 -o test_generic_host
! run with 
! ./test_generic_host < test.in
! To test the dectris-neggia library, one could use this test.in:
!/usr/local/lib64/dectris-neggia.so
!/scratch/data/Eiger_16M_Nov2015/2015_11_10/insu6_1_??????.h5
!1 900
!
! The OMP_NUM_THREADS environment variable may be used for benchmarks!


PROGRAM test_generic_host
    USE generic_data_plugin, ONLY: library, firstqm, lastqm, nx, ny, is_open, &
        generic_open, generic_get_header, generic_get_data, generic_close
    IMPLICIT            NONE
    INTEGER            :: ier,nxny,ilow,ihigh,nbyte,info_array(1024),  &
                          number_of_frames,len,numfrm
    INTEGER, ALLOCATABLE :: iframe(:)
    REAL               :: qx,qy,avgcounts
    CHARACTER(len=:), ALLOCATABLE :: master_file
    CHARACTER(len=512) :: ACTNAM

! what should be done?
    WRITE(*,*)'enter parameter of LIB= keyword:'
    READ(*,'(a)') actnam
    library=TRIM(actnam)
    WRITE(*,*)'enter parameter of NAME_TEMPLATE_OF_DATA_FRAMES= keyword:'
    READ(*,'(a)') actnam
    WRITE(*,*)'enter parameters of the DATA_RANGE= keyword:'
    READ(*,*) ilow,ihigh
    
! set some more module variables
    firstqm=INDEX(actnam,'?')   ! qm means question mark
    lastqm =INDEX(actnam,'?',BACK=.TRUE.)
    len    =LEN_TRIM(actnam)
    IF (actnam(len-2:len)=='.h5')THEN
      master_file=actnam(:len-9)//'master.h5'
      PRINT*,'master_file=',TRIM(master_file)
    ELSE
       master_file=TRIM(actnam)
    ENDIF
    info_array(1) = 1         ! 1=XDS  (generic_open may check this)
    info_array(2) = 123456789 ! better: e.g. 20160510; generic_open may check this

! initialize
    CALL generic_open(library, master_file,info_array, ier)
    IF (ier<0) THEN
      WRITE(*,*)'error from generic_open, ier=',ier       
      STOP
    END IF
    is_open=.TRUE.

! get header and report
    CALL generic_get_header(nx,ny,nbyte,qx,qy,number_of_frames,info_array,ier)
    IF (ier<0) THEN
      WRITE(*,*)'error from generic_get_header, ier=',ier       
      STOP
    END IF
    WRITE(*,'(a,3i6,2f10.6,i6)')'nx,ny,nbyte,qx,qy,number_of_frames=', &
                                 nx,ny,nbyte,qx,qy,number_of_frames
    WRITE(*,'(a,4i4,i12)')'INFO(1:5)=vendor/major version/minor version/patch/timestamp=', &
         info_array(1:5)
    IF (info_array(1)==0) THEN
       WRITE(*,*) 'generic_getfrm: data are not vendor-specific',info_array(1) ! 1=Dectris
    ELSE IF (info_array(1)==1) THEN
      WRITE(*,*) 'generic_getfrm: data are from Dectris'
    END IF
    nxny=nx*ny
    avgcounts=0.
    
! read the data (possibly in parallel)
!$omp parallel default(shared) private(numfrm,iframe,info_array,ier)
    ALLOCATE(iframe(nxny))
!$omp do reduction(+:avgcounts)
    DO numfrm=ilow,ihigh
      CALL generic_get_data(numfrm, nx, ny, iframe, info_array, ier)
      IF (ier<0) THEN
        WRITE(*,*)'error from generic_get_data, numfrm, ier=',numfrm,ier       
        STOP
      END IF
      avgcounts=avgcounts + SUM(iframe)/REAL(nxny) ! do something with data
    END DO
!$omp end parallel
    WRITE(*,*)'average counts:',avgcounts/(ihigh-ilow+1)
    
! close
    CALL generic_close(ier)
    IF (ier<0) THEN
      WRITE(*,*)'error from generic_close, ier=',ier       
      STOP
    END IF
    
END PROGRAM test_generic_host
