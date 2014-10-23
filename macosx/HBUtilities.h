/*  HBUtilities.h $

 This file is part of the HandBrake source code.
 Homepage: <http://handbrake.fr/>.
 It may be used under the terms of the GNU General Public License. */

#import <Foundation/Foundation.h>

@interface HBUtilities : NSObject

/**
 *  Returns the path of the current <user>/Library/Application Support/HandBrake folder.
 */
+ (NSString *)appSupportPath;

/**
 *  Writes a message to standard error.
 *  The message will show up in the output panel and in the activity log.
 *
 *  @param format a standard c format string with varargs.
 */
+ (void)writeToActivityLog:(const char *)format, ...;

/**
 *  Generates a file name automatically based on the inputs,
 *  it can be configured with NSUserDefaults.
 *
 *  @param sourceName    the name of the source file
 *  @param title         the title number
 *  @param chaptersRange the selected chapters range
 *  @param quality       the video encoder quality
 *  @param bitrate       the video encoder bitrate
 *  @param videoCodec    the video encoder type
 *
 *  @return a NSString containing the required info
 */
+ (NSString *)automaticNameForSource:(NSString *)sourceName
                               title:(NSUInteger)title
                            chapters:(NSRange)chaptersRange
                             quality:(NSString *)quality
                             bitrate:(NSString *)bitrate
                          videoCodec:(uint32_t)codec;

@end
