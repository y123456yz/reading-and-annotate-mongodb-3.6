
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


