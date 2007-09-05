PROJECT_ROOT="../../.."
BOOST_SRC_DIR=~/svn/boost_docs/
BOOSTBOOK_TOOL_DIR=$(BOOST_SRC_DIR)/tools/boostbook
.PHONY: all
all: doc

.PHONY: clean
clean:
	$(RM) -r html/*
	$(RM) libpoet_reference.docbook

.PHONY: doc
doc: html

.PHONY: html
html: libpoet_reference.docbook
	(export XML_CATALOG_FILES="./catalog.xml /etc/xml/catalog";\
		xsltproc --xinclude --stringparam project.root $(PROJECT_ROOT) -o html/ $(BOOSTBOOK_TOOL_DIR)/xsl/html.xsl $<)

libpoet_reference.docbook: libpoet_reference.xml
	(export XML_CATALOG_FILES="./catalog.xml /etc/xml/catalog";\
		xsltproc --xinclude --stringparam project.root $(PROJECT_ROOT) -o $@ $(BOOSTBOOK_TOOL_DIR)/xsl/docbook.xsl $<)
