#!/bin/bash

FILEPATH='/ppro/data/'
FILENAME=""
RECORD_LENGTH=0 # Length of file, as reported by XXXDEF.TXT (data file definition) files
FULL_MODE='N' # Full file traversal (without filechk call)
HELP_MODE='N'
HEX_MODE='N'
UPDATE_MODE='N'
VALID_NUM='^[0-9]+$'

for i in "$@"
do
case $i in
	
    -f=*|--filename=*)
    FILENAME="${i#*=}"
    shift # past argument=value
    ;;

    -d=*|--file_directory=*)
    FILEPATH="${i#*=}"
    shift # past argument=value
    ;;

    -h|--help)
    HELP_MODE='Y'
    shift # past argument=value
    ;;

    -l=*|--length=*)
    RECORD_LENGTH="${i#*=}"
    shift # past argument=value
    ;;

	-u|--update)
    UPDATE_MODE='Y'
    shift # past argument=value
    ;;

    -x|--hex_mode)
    HEX_MODE='Y'
    shift # past argument=value
    ;;

    -y|--full_mode)
	FULL_MODE='Y'
	shift
	;;

    *)
            # unknown option
    ;;
esac
done

if ! [[ $RECORD_LENGTH =~ $VALID_NUM ]]; then
	echo "Error: $RECORD_LENGTH is not a number!"
	echo "Aborting program."
	exit 1
elif [[ "$HELP_MODE" = "Y" ]]; then
	echo "SYNOPSIS"
	echo "		./filefix.sh [OPTIONS..]"
	echo ""
	echo "DESCRIPTION"
	echo "		Perform data file invalid character detection. Mandatory"
	echo "		options are indicated with an asterisk (*). Default behavior"
	echo "		is to utilize DB/C utility, filechk, to calculate wrong record"
	echo "		length positions based on indicated record length without"
	echo "		performing any updates."
	echo ""
	echo "		-f, --filename *"
	echo "			Full, case-sensitive filename excluding path."
	echo ""
	echo "		-d, --file_directory"
	echo "			File directory of file. Requires terminating"
	echo "			'/' (eg. '/ppro/data/'). Defaults to /ppro/data/"
	echo "			if no input is provided."
	echo ""
	echo "		-h, --help"
	echo "			Display help information."
	echo ""
	echo "		-l, --length *"
	echo "			Set record length of data file, as reported in the data"
	echo "			file definition (XXXDEF.TXT)."
	echo ""
	echo "		-u, --update"
	echo "			Set update mode to perform data file updates."
	echo ""
	echo "		-x, --hex_mode"
	echo "			Run in 0x00 detection mode."
	echo ""
	echo "		-y, --full_mode"
	echo "			Set record length of data file."
	echo ""
    echo " Sample syntax:"
    echo "		./filefix.sh -f=SOH0007.TXT -l=1851 -d=/ppro/pprotest/data/ -u"
    echo ""
    echo " Note:"
    echo "		Data file record sizes are actually one character longer due to"
    echo "		the use of a record-terminating 0xFA character."
	exit 0
elif [[ "$RECORD_LENGTH" = "0" ]]; then
	echo "Error: No record length specified."
	echo "Aborting program."
	exit 1
fi

echo "File path: $FILEPATH$FILENAME"
echo "Record length: $RECORD_LENGTH"
echo "Update mode: $UPDATE_MODE"
echo "Hex mode: $HEX_MODE"
echo "Full mode: $FULL_MODE"
echo ""

if [[ "$FULL_MODE" = "Y" ]]; then
	if [[ "$UPDATE_MODE" = "Y" ]]; then
		if [[ "$HEX_MODE" = "Y" ]]; then
			/ppro/mtl/bin/compile/filefix -d $FILEPATH$FILENAME -l $((RECORD_LENGTH + 1)) -x -y -u
		else
			/ppro/mtl/bin/compile/filefix -d $FILEPATH$FILENAME -l $((RECORD_LENGTH + 1)) -y -u
		fi
	else
		if [[ "$HEX_MODE" = "Y" ]]; then
			/ppro/mtl/bin/compile/filefix -d $FILEPATH$FILENAME -l $((RECORD_LENGTH + 1)) -x -y
		else
			/ppro/mtl/bin/compile/filefix -d $FILEPATH$FILENAME -l $((RECORD_LENGTH + 1)) -y
		fi
	fi
else
	filechk $FILEPATH$FILENAME -L=$RECORD_LENGTH | awk '/Wrong length record/ {getline; print $4}' | awk -F '[^0-9]*' '$0=$2' > filefix.in
	if [[ "$UPDATE_MODE" = "Y" ]]; then
		if [[ "$HEX_MODE" = "Y" ]]; then
			/ppro/mtl/bin/compile/filefix -d $FILEPATH$FILENAME -l $((RECORD_LENGTH + 1)) -x -i filefix.in -u
		else
			/ppro/mtl/bin/compile/filefix -d $FILEPATH$FILENAME -l $((RECORD_LENGTH + 1)) -i filefix.in -u
		fi
	else
		if [[ "$HEX_MODE" = "Y" ]]; then
			/ppro/mtl/bin/compile/filefix -d $FILEPATH$FILENAME -l $((RECORD_LENGTH + 1)) -x -i filefix.in
		else
			/ppro/mtl/bin/compile/filefix -d $FILEPATH$FILENAME -l $((RECORD_LENGTH + 1)) -i filefix.in
		fi
	fi
fi
