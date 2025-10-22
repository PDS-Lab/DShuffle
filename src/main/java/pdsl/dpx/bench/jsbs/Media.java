package pdsl.dpx.bench.jsbs;

import java.util.List;
import java.io.Serializable;

public class Media implements Serializable {
    public enum Player {
        JAVA, FLASH,
    }

    public String uri;
    public String title;
    public int width;
    public int height;
    public String format;
    public long duration;
    public long size;
    public int bitrate;
    public boolean hasBitrate;
    public List<String> persons;
    public Player player;
    public String copyright;


    @Override
    public boolean equals(Object o) {
        if (this == o)
            return true;
        if (o == null || getClass() != o.getClass()) {
            System.err.println("ah?");
            return false;
        }

        Media media = (Media) o;

        if (bitrate != media.bitrate) {
            System.err.println("bitrate?");
            return false;
        }
        if (duration != media.duration) {
            System.err.println("duration?");
            return false;
        }
        if (hasBitrate != media.hasBitrate) {
            System.err.println("hasBitrate?");
            return false;
        }
        if (height != media.height) {
            System.err.println("height?");
            return false;
        }
        if (size != media.size) {
            System.err.println("size?");
            return false;
        }
        if (width != media.width) {
            System.err.println("width?");
            return false;
        }
        if (copyright != null ? !copyright.equals(media.copyright) : media.copyright != null) {
            System.err.println("copyright?");
            return false;
        }
        if (format != null ? !format.equals(media.format) : media.format != null) {
            System.err.println("format?");
            return false;
        }
        if (persons != null ? !persons.equals(media.persons) : media.persons != null) {
            System.err.println("persons?");
            return false;
        }
        if (player != media.player) {
            System.err.println("hasBitrate?");
            return false;
        }
        if (title != null ? !title.equals(media.title) : media.title != null) {
            System.err.println("hasBitrate?");
            return false;
        }
        if (uri != null ? !uri.equals(media.uri) : media.uri != null) {
            System.err.println("hasBitrate?");
            return false;
        }

        return true;
    }

    @Override
    public String toString() {
        String s = String.format("%s\n", getClass().toString());
        s += String.format("uri: %s\n", uri);
        s += String.format("title: %s\n", title);
        s += String.format("width: %d\n", width);
        s += String.format("height: %d\n", height);
        s += String.format("format: %s\n", format);
        s += String.format("duration: %d\n", duration);
        s += String.format("size: %d\n", size);
        s += String.format("bitrate: %d\n", bitrate);
        s += String.format("hasBitrate: %b\n", hasBitrate);
        for (int i = 0; i < persons.size(); i++) {
            s += String.format("persons[%d]: %s\n", i, persons.get(i));
        }
        s += String.format("player: %s\n", player == null ? null : player.toString());
        s += String.format("copyright: %s\n", copyright);
        return s;
    }

}
