PostgreSQL support for GDAL VFK driver
======================================

How to compile
--------------

        svn co https://svn.osgeo.org/gdal/trunk/gdal
        cd gdal
        
        (cd ogr/ogrsf_frmts/ &&
         mv vfk vfk.orig &&
         git clone https://github.com/ctu-geoforall-lab-sandbox/gdal-vfk-pg vfk)
        
        ./configure --prefix=/usr/local --with-sqlite3 --with-pg
        make
        sudo make install

References
----------

https://github.com/ctu-yfsg/2016-a-gdal-vfk-pg
