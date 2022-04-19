FROM ubuntu:20.04 AS compiler-common
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
&& apt-get install -y --no-install-recommends \
 git-core \
 checkinstall \
 g++ \
 make \
 tar \
 wget \
 ca-certificates

###########################################################################################################

FROM compiler-common AS compiler-postgis
RUN apt-get install -y --no-install-recommends \
 postgresql-server-dev-12 \
 libxml2-dev \
 libgeos-dev \
 libproj-dev
RUN wget https://download.osgeo.org/postgis/source/postgis-3.1.1.tar.gz -O postgis.tar.gz \
&& mkdir -p postgis_src \
&& tar -xvzf postgis.tar.gz --strip 1 -C postgis_src \
&& rm postgis.tar.gz \
&& cd postgis_src \
&& ./configure --without-protobuf --without-raster \
&& make -j $(nproc) \
&& checkinstall --pkgversion="3.1.1" --install=no --default make install

###########################################################################################################

FROM compiler-common AS compiler-osm2pgsql
RUN apt-get install -y --no-install-recommends \
 cmake \
 libboost-dev \
 libboost-system-dev \
 libboost-filesystem-dev \
 libexpat1-dev \
 zlib1g-dev \
 libbz2-dev \
 libpq-dev \
 libproj-dev \
 lua5.3 \
 liblua5.3-dev \
 pandoc
RUN cd ~ \
&& git clone -b master --single-branch https://github.com/openstreetmap/osm2pgsql.git --depth 1 \
&& cd osm2pgsql \
&& mkdir build \
&& cd build \
&& cmake .. \
&& make -j $(nproc) \
&& checkinstall --pkgversion="1" --install=no --default make install

###########################################################################################################

FROM compiler-common AS compiler-modtile-renderd
RUN apt-get install -y --no-install-recommends \
 apache2-dev \
 automake \
 autoconf \
 autotools-dev \
 libtool \
 libmapnik-dev
RUN cd ~ \
&& git clone -b switch2osm --single-branch https://github.com/SomeoneElseOSM/mod_tile.git --depth 1 \
&& cd mod_tile \
&& ./autogen.sh \
&& ./configure \
&& make -j $(nproc) \
&& checkinstall --pkgversion="1" --install=no --pkgname "renderd" --default make install \
&& checkinstall --pkgversion="1" --install=no --pkgname "mod_tile" --default make install-mod_tile

###########################################################################################################

FROM compiler-common AS compiler-stylesheet
RUN apt-get install -y --no-install-recommends \
 npm \
 python-is-python3 \
 python3-distutils \
 nodejs \
 curl \
 wget
RUN cd ~ \
&& git clone https://github.com/traitor6789/openstreetmap-carto.git \
&& chmod -R 777 openstreetmap-carto \
&& cd openstreetmap-carto \
&& sed -ie 's#https:\/\/naciscdn.org\/naturalearth\/110m\/cultural\/ne_110m_admin_0_boundary_lines_land.zip#https:\/\/naturalearth.s3.amazonaws.com\/110m_cultural\/ne_110m_admin_0_boundary_lines_land.zip#g' external-data.yml \
&& npm install -g carto@0.18.2 \
&& carto project.mml > mapnik.xml \
&& chmod -R 777 scripts/get-shapefiles.py \
&& ls -al \
&& scripts/get-shapefiles.py -s && cd ..

###########################################################################################################

FROM compiler-common AS compiler-helper-script
RUN mkdir -p /home/renderer/src \
&& cd /home/renderer/src \
&& git clone https://github.com/zverik/regional \
&& cd regional \
&& git checkout 889d630a1e1a1bacabdd1dad6e17b49e7d58cd4b \
&& rm -rf .git \
&& chmod u+x /home/renderer/src/regional/trim_osc.py

###########################################################################################################

FROM ubuntu:20.04 AS final-base

# Based on
# https://switch2osm.org/serving-tiles/manually-building-a-tile-server-18-04-lts/
ENV TZ=UTC
ENV DEBIAN_FRONTEND=noninteractive
ENV AUTOVACUUM=off
ENV UPDATES=disabled
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# Install dependencies
COPY  /dependencies.sh / 
RUN ["chmod","+x","/dependencies.sh"]
RUN /dependencies.sh

RUN adduser --disabled-password --gecos "" renderer

# Install python libraries
RUN pip3 install \
 requests \
 pyyaml

# Install and test Mapnik
RUN python -c 'import mapnik'

# Set up PostGIS
RUN wget https://download.osgeo.org/postgis/source/postgis-3.1.1.tar.gz
RUN tar -xvzf postgis-3.1.1.tar.gz
RUN cd postgis-3.1.1 && ./configure && make && make install

# Configure Apache
RUN mkdir /var/lib/mod_tile \
&& chown renderer /var/lib/mod_tile \
&& mkdir /var/run/renderd \
&& chown renderer /var/run/renderd \
&& echo "LoadModule tile_module /usr/lib/apache2/modules/mod_tile.so" >> /etc/apache2/conf-available/mod_tile.conf \
&& echo "LoadModule headers_module /usr/lib/apache2/modules/mod_headers.so" >> /etc/apache2/conf-available/mod_headers.conf \
&& a2enconf mod_tile && a2enconf mod_headers
COPY apache.conf /etc/apache2/sites-available/000-default.conf
COPY leaflet-demo.html /var/www/html/index.html
COPY leaflet.css /var/www/html/leaflet.css
COPY leaflet.js /var/www/html/leaflet.js
RUN ln -sf /dev/stdout /var/log/apache2/access.log \
&& ln -sf /dev/stderr /var/log/apache2/error.log

# Copy update scripts
COPY openstreetmap-tiles-update-expire /usr/bin/
RUN chmod +x /usr/bin/openstreetmap-tiles-update-expire \
&& mkdir /var/log/tiles \
&& chmod a+rw /var/log/tiles \
&& ln -s /home/renderer/src/mod_tile/osmosis-db_replag /usr/bin/osmosis-db_replag \
&& echo "* * * * *   renderer    openstreetmap-tiles-update-expire\n" >> /etc/crontab

RUN mkdir /nodes \
&& chown renderer:renderer /nodes

# Configure PosgtreSQL
COPY postgresql.custom.conf.tmpl /etc/postgresql/12/main/
RUN chown -R postgres:postgres /var/lib/postgresql \
&& chown postgres:postgres /etc/postgresql/12/main/postgresql.custom.conf.tmpl \
&& echo "host all all 0.0.0.0/0 md5" >> /etc/postgresql/12/main/pg_hba.conf \
&& echo "host all all ::/0 md5" >> /etc/postgresql/12/main/pg_hba.conf

#search for the data files in Data folder

CMD file_pbf=$(find Data/malta-latest.osm.pbf" -printf "%f\n")
CMD file_poly=$(find Data/malta.poly" -printf "%f\n")

#move the data files to the container
RUN mkdir -p  /home/renderer/src/Data 
COPY  /Data/$file_pbf   /home/renderer/src/Data/
COPY  /Data/$file_poly  /var/lib/mod_tile/file_poly/

###########################################################################################################

FROM final-base AS final

# Install PostGIS
COPY --from=compiler-postgis postgis_src/postgis-src_3.1.1-1_amd64.deb .
RUN ls -al
RUN dpkg -i postgis-src_3.1.1-1_amd64.deb \
&& chmod +x postgis-src_3.1.1-1_amd64.deb && ls -al \
&& rm postgis-src_3.1.1-1_amd64.deb

# Install osm2pgsql
COPY --from=compiler-osm2pgsql /root/osm2pgsql/build/build_1-1_amd64.deb .
RUN dpkg -i build_1-1_amd64.deb \
&& rm build_1-1_amd64.deb

# Install renderd
COPY --from=compiler-modtile-renderd /root/mod_tile/renderd_1-1_amd64.deb .
RUN dpkg -i renderd_1-1_amd64.deb \
&& rm renderd_1-1_amd64.deb \
&& sed -i 's/renderaccount/renderer/g' /usr/local/etc/renderd.conf \
&& sed -i 's/\/truetype//g' /usr/local/etc/renderd.conf \
&& sed -i 's/hot/tile/g' /usr/local/etc/renderd.conf

# Install mod_tile
COPY --from=compiler-modtile-renderd /root/mod_tile/mod-tile_1-1_amd64.deb .
RUN dpkg -i mod-tile_1-1_amd64.deb \
 && ldconfig \
 && rm mod-tile_1-1_amd64.deb
COPY --from=compiler-modtile-renderd /root/mod_tile/osmosis-db_replag /usr/bin/osmosis-db_replag

# Install stylesheet
COPY --from=compiler-stylesheet /root/openstreetmap-carto /home/renderer/src/openstreetmap-carto

# Install helper script
COPY --from=compiler-helper-script /home/renderer/src/regional /home/renderer/src/regional

# Start running
COPY run.sh /
ENTRYPOINT ["/run.sh"]
CMD []
EXPOSE 80 5432
