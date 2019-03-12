
BSONObj data;
{
	BSONObjBuilder b;
	b.append("numRecords", entry.numRecords);
	b.append("dataSize", entry.dataSize);
	data = b.obj();
}

LOG(2) << "WiredTigerSizeStorer::storeInto " << uriKey << " -> " << redact(data);

StatusWith<RecordId> res =
	  _rs->insertRecord(opCtx, obj.objdata(), obj.objsize(), Timestamp(), enforceQuota);
  if (!res.isOK())
	  return res.getStatus();

  old = Entry(ident, res.getValue());
  LOG(1) << "stored meta data for " << ns << " @ " << res.getValue();

  log() << "build index on: " << ns << " properties: " << descriptor->toString();


const StringData key(keyItem.str, keyItem.len);
if (keysSeen.count(key)) {
  return Status(ErrorCodes::DuplicateKey,
				str::stream() << "app_metadata must not contain duplicate keys. "
							  << "Found multiple instances of key '"
							  << key
							  << "'.");
}

int retCode, const char* prefix
str::stream s;
if (prefix)
   s << prefix << " ";
s << retCode << ": " << wiredtiger_strerror(retCode);


WT_CONFIG_ITEM keyItem;
WT_CONFIG_ITEM valueItem;
int ret;
auto keysSeen = SimpleStringDataComparator::kInstance.makeStringDataUnorderedSet();
while ((ret = parser.next(&keyItem, &valueItem)) == 0) {
    const StringData key(keyItem.str, keyItem.len);
    if (keysSeen.count(key)) {
        return Status(ErrorCodes::DuplicateKey,
                      str::stream() << "app_metadata must not contain duplicate keys. "
                                    << "Found multiple instances of key '"
                                    << key
                                    << "'.");

str::stream() << "'formatVersion' in application metadata for " << uri
									<< " must be a number. Current value: "
									<< StringData(versionItem.str, versionItem.len));


const char* message
error() << "WiredTiger error (" << errorCode << ") " << redact(message);



