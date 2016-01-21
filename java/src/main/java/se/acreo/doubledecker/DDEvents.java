package se.acreo.doubledecker;

public interface DDEvents {

    void registered(String endpoint);

    void disconnected(String endpoint);

    void publish(String source, String topic, byte[] data);

    void data(String source, byte[] data);

    void error(int code, String reason);
}
