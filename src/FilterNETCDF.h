//LabPlot : FilterNETCDF.h

#ifndef FILTERNETCDF_H
#define FILTERNETCDF_H

#include <qstring.h>
#include <qstringlist.h>

#ifdef HAVE_NETCDF
#include <netcdf.h>

struct ncdim {			// dimension
    char name[NC_MAX_NAME];
    size_t size;
};

struct ncvar {				// variable
    char name[NC_MAX_NAME];
    nc_type type;
    int ndims;
    int dims[MAX_VAR_DIMS];
    int natts;
    bool has_fillval;
    double fillval;
};

struct ncatt {				// attribute
    int var;
    char name[NC_MAX_NAME];
    nc_type type;
    size_t len;
    char *string;				// for text attributes (type = NC_CHAR)
    double *vals;			// for numeric attributes of all types
};
#endif

class FilterNETCDF
{
public:
	FilterNETCDF(QString filename=0);
	bool fileOK() const { return fileok; }
#ifdef HAVE_NETCDF
	QString version() const { return QString(nc_inq_libvers());}
	int NDims() const { return ndims; }
	int NVars() const { return nvars; }
	int NAtts() const { return natts; }
	int DimId() const { return xdimid; }
	QString Dim(int dimid) const;				// get dimension dimid
	QString Var(int varid);					// get variable varid description
	QString VarName(int varid) const;			// get variable varid name
	QString Att(int aid) { return pr_att(ncid, NC_GLOBAL, "", aid); } // get attribute aid
	int VarLen(QString var) const;				// get length of variable var
	QStringList DataString(int varid) const;		// get variable data as string
	double Data(QString var,size_t index) const;		// get data for variable var at index
protected:
	QString typeName(nc_type type) const;
	QString pr_att_string(size_t len,const char *string);
	QString pr_att_vals(nc_type type,size_t len,const double *vals);
	QString pr_att(int ncid,int varid,const char *varname, int ia);
#endif
private:
	bool fileok;				//!< is a netcdf file and readable
	QString filename;
#ifdef HAVE_NETCDF
	int ncid, ndims, nvars, natts, xdimid;	// netcdf id / number of dimensions / vars / attributes / dim id
	ncdim dims[NC_MAX_DIMS];			// nc dimensions
#endif
};

#endif //FILTERNETCDF_H
