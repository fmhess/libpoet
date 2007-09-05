PROJECT_ROOT="../../.."
BOOST_SRC_DIR=~/svn/boost_docs/
BOOSTBOOK_TOOL_DIR=$(BOOST_SRC_DIR)/tools/boostbook

XML_SRCS=\
	active_function_hpp.xml \
	active_object_hpp.xml \
	exception_ptr_hpp.xml \
	exceptions_hpp.xml \
	future_hpp.xml \
	libpoet_doc.xml \
	libpoet_reference.xml

.PHONY: all
all: doc

.PHONY: clean
clean:
	$(RM) -r html/*
	$(RM) libpoet_doc.docbook

.PHONY: doc
doc: html

.PHONY: html
html: libpoet_doc.docbook
	(export XML_CATALOG_FILES="./catalog.xml /etc/xml/catalog";\
		xsltproc --xinclude --stringparam project.root $(PROJECT_ROOT) -o html/ $(BOOSTBOOK_TOOL_DIR)/xsl/html.xsl $<)

libpoet_doc.docbook: $(XML_SRCS)
	(export XML_CATALOG_FILES="./catalog.xml /etc/xml/catalog";\
		xsltproc --xinclude --stringparam project.root $(PROJECT_ROOT) -o $@ $(BOOSTBOOK_TOOL_DIR)/xsl/docbook.xsl libpoet_doc.xml)
