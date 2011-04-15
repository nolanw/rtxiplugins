/*
 * Grab samples from a file in chunks, on request.
 */

#ifndef WORKER_H_X5J7EA96
#define WORKER_H_X5J7EA96

#include <stdint.h>
#include <vector>

#include <boost/iostreams/device/mapped_file.hpp>

class SamplePlayer;

#include <qthread.h>
class QSemaphore;

class SampleWorker : public QThread
{
public:
  SampleWorker(QString aFilename, SamplePlayer *aPlayer);
  virtual ~SampleWorker();
  virtual void run();
  void fetchMoreSamples();
  void bail();
  std::vector<double> samples;
  
private:
  QSemaphore advance;
  QString filename;
  SamplePlayer *player;
  uintmax_t filesize;
  boost::iostreams::mapped_file sampleFile;
  boost::intmax_t nextOffset;
  bool _bail;
};

#endif /* end of include guard: WORKER_H_X5J7EA96 */
