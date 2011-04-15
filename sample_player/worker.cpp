#include "worker.h"

#include <algorithm>
#include <iostream>

#include <boost/filesystem/operations.hpp>

#include "sample_player.h"

#include <qapplication.h>
#include <qsemaphore.h>

// Grab points in batches of three hundred thousand, a number scientifically 
// determined by process of sounding right.
#define WINDOW_SIZE (300000 * sizeof(double))

namespace bio = boost::iostreams;
namespace bfs = boost::filesystem;

SampleWorker::SampleWorker(QString aFilename, SamplePlayer *aPlayer) :
  advance(1), filename(aFilename), player(aPlayer), nextOffset(0), 
  _bail(false)
{
  bfs::path filepath(aFilename.utf8(), bfs::native);
  if (bfs::exists(filepath) && bfs::is_regular_file(filepath))
  {
    filesize = bfs::file_size(filepath);
  }
  else
  {
    filesize = 0;
  }
}

SampleWorker::~SampleWorker()
{
}

void SampleWorker::fetchMoreSamples()
{
  advance--;
}

void SampleWorker::bail()
{
  _bail = true;
  advance--;
}

void SampleWorker::run()
{
  if (filesize == 0)
  {
    return;
  }
  for (;;)
  {
    advance++;
    if (_bail)
    {
      break;
    }
    samples.clear();
    if (sampleFile.is_open())
    {
      sampleFile.close();
    }
    sampleFile.open(filename, 
      std::ios_base::binary | std::ios_base::in, 
      std::min(WINDOW_SIZE, filesize - nextOffset), 
      nextOffset);
    nextOffset += WINDOW_SIZE;
    nextOffset -= (nextOffset % bio::mapped_file::alignment());
    nextOffset += bio::mapped_file::alignment();
    if (nextOffset >= (boost::intmax_t)filesize)
    {
      break;
    }
    double *unreadSamples = (double *)sampleFile.const_data();
    for (size_t i = 0; i < sampleFile.size() / sizeof(double); i++)
    {
      samples.push_back(unreadSamples[i]);
    }
    QApplication::postEvent(player, new JobDoneEvent());
  }
}
