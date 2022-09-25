#ifndef RECENTFILELIST_H
#define RECENTFILELIST_H

#include <QStringList>

class RecentFileList: public QStringList
{
  public:
    using QStringList::QStringList;

    inline void addFile(const QString &file)
    {
        // If an item exist, remove it from the list and reinsert it at the beginning.
        removeAll(file);

        if(!file.isEmpty()) prepend(file);
        if(count() > maxNofRecentFiles) erase(begin() + maxNofRecentFiles, end());
    }

  private:
    constexpr static int maxNofRecentFiles = 10;
};

#endif /* RECENTFILELIST_H */
